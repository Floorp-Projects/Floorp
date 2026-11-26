/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * navigator.clipboard.read()/readText() for web panels.
 *
 * Gecko asks the user to confirm a paste only when the requesting content is
 * in the focused tab: nsBaseClipboard::RequestUserConfirmation() compares the
 * top chrome document's activeElement with the content's embedder <browser>.
 * A web panel's <browser> lives in a nested browser.xhtml, so the top chrome
 * document's activeElement is the panel's outer <browser> and every read that
 * needs confirmation is rejected with "Clipboard read operation is not
 * allowed." without showing the paste menu.
 *
 * In web panel pages we wrap read()/readText() from a sandbox that has the
 * page's principal, so the original methods still run with the page as the
 * subject. Only that specific rejection falls back to the parent actor, which
 * repeats Gecko's focus check across the nested window and shows Gecko's own
 * paste confirmation menu.
 */

import {
  isWebPanelBrowserId,
  isWebPanelClipboardReadKind,
  NR_WEB_PANEL_CLIPBOARD_READ_MESSAGE,
  type NRWebPanelClipboardReadResult,
  WEB_PANEL_BROWSER_IDS_SHARED_KEY,
} from "../common/NRWebPanelClipboardTypes.ts";

/**
 * Evaluates, in a sandbox whose prototype is the page window and whose
 * principal is the page's, to a function that installs the wrappers before
 * any page script runs. `window` is the page window without Xrays, so the
 * created promises, arrays, blobs and errors belong to the page.
 *
 * Everything the wrappers call is captured at install time and invoked
 * through the captured `Reflect.apply`, so later changes to the page's (or
 * this sandbox's) built-ins cannot redirect them. `request` stays in this
 * closure and is never reachable from the sandbox global.
 */
export const WEB_PANEL_CLIPBOARD_SANDBOX_SOURCE = `(function (request) {
  "use strict";
  const apply = Reflect.apply;
  const win = window;
  const proto = win.Clipboard && win.Clipboard.prototype;
  if (!proto) {
    return;
  }

  const PagePromise = win.Promise;
  const then = PagePromise.prototype.then;
  const PageArray = win.Array;
  const arrayOf = PageArray.of;
  const PageObject = win.Object;
  const PageBlob = win.Blob;
  const PageClipboardItem = win.ClipboardItem;
  const PageDOMException = win.DOMException;
  const defineProperty = Object.defineProperty;
  const getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;

  const NOT_ALLOWED = "Clipboard read operation is not allowed.";
  const NO_ACTIVATION =
    "Clipboard read request was blocked due to lack of user activation.";

  function toReadResult(kind, entries) {
    if (kind === "readText") {
      for (let i = 0; i < entries.length; i++) {
        if (entries[i].type === "text/plain") {
          return "" + (entries[i].text ?? "");
        }
      }
      return "";
    }
    if (!entries.length) {
      return new PageArray();
    }
    const record = new PageObject();
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      const part = entry.bytes ?? entry.text ?? "";
      record[entry.type] = new PageBlob([part], { type: entry.type });
    }
    return apply(arrayOf, PageArray, [new PageClipboardItem(record)]);
  }

  function fallback(kind) {
    return new PagePromise((resolve, reject) => {
      request(kind, (response) => {
        if (response && response.status === "ok") {
          try {
            resolve(toReadResult(kind, response.entries));
          } catch (error) {
            reject(error);
          }
          return;
        }
        const message = response && response.status === "no-activation"
          ? NO_ACTIVATION
          : NOT_ALLOWED;
        reject(new PageDOMException(message, "NotAllowedError"));
      });
    });
  }

  function wrap(kind) {
    const descriptor = getOwnPropertyDescriptor(proto, kind);
    if (!descriptor || typeof descriptor.value !== "function") {
      return;
    }
    const original = descriptor.value;
    // A method keeps the native name and length and is not constructible.
    const wrapped = {
      [kind]() {
        return apply(then, apply(original, this, arguments), [
          undefined,
          (error) => {
            if (
              !error || error.name !== "NotAllowedError" ||
              error.message !== NOT_ALLOWED
            ) {
              throw error;
            }
            return fallback(kind);
          },
        ]);
      },
    }[kind];
    defineProperty(proto, kind, { ...descriptor, value: wrapped });
  }

  wrap("read");
  wrap("readText");
})`;

export class NRWebPanelClipboardChild extends JSWindowActorChild {
  handleEvent(event: Event): void {
    if (event.type !== "DOMDocElementInserted") {
      return;
    }
    if (event.target !== this.document) {
      return;
    }
    // Runs for every http(s) document: skip the structured-clone read of the
    // list unless at least one web panel is open.
    const sharedData = Services.cpmm.sharedData;
    if (
      !sharedData?.has(WEB_PANEL_BROWSER_IDS_SHARED_KEY) ||
      !isWebPanelBrowserId(
        sharedData.get(WEB_PANEL_BROWSER_IDS_SHARED_KEY),
        this.browsingContext?.browserId,
      )
    ) {
      return;
    }

    try {
      this.installClipboardFallback();
    } catch (error) {
      console.error("[NRWebPanelClipboard]", error);
    }
  }

  private installClipboardFallback(): void {
    const win = this.contentWindow;
    const principal = this.document?.nodePrincipal;
    if (!win?.isSecureContext || !principal) {
      return;
    }

    // Same principal as the page and no Xrays: the original clipboard methods
    // see the page as the caller, and the page sees the wrappers as its own.
    const sandbox = Cu.Sandbox(principal, {
      sandboxName: "Floorp web panel clipboard",
      sandboxPrototype: win,
      sameZoneAs: win,
      wantXrays: false,
    });
    const install = Cu.evalInSandbox(
      WEB_PANEL_CLIPBOARD_SANDBOX_SOURCE,
      sandbox,
    ) as (request: unknown) => void;
    install(
      Cu.exportFunction(
        (kind: unknown, callback: (response: unknown) => void) =>
          this.requestPaste(kind, callback, sandbox),
        sandbox,
      ),
    );
  }

  private requestPaste(
    kind: unknown,
    callback: (response: unknown) => void,
    sandbox: object,
  ): void {
    const respond = (response: NRWebPanelClipboardReadResult) => {
      try {
        callback(Cu.cloneInto(response, sandbox));
      } catch (error) {
        console.error("[NRWebPanelClipboard]", error);
      }
    };

    if (typeof callback !== "function") {
      return;
    }
    if (!isWebPanelClipboardReadKind(kind)) {
      respond({ status: "denied" });
      return;
    }
    // Gecko checked activation before rejecting and does not consume it.
    // Check again so the fallback cannot be used without a user gesture.
    if (!this.document?.hasValidTransientUserGestureActivation) {
      respond({ status: "no-activation" });
      return;
    }

    this.sendQuery(NR_WEB_PANEL_CLIPBOARD_READ_MESSAGE, { kind }).then(
      (response: NRWebPanelClipboardReadResult | null | undefined) =>
        respond(response ?? { status: "denied" }),
      (error: unknown) => {
        console.error("[NRWebPanelClipboard]", error);
        respond({ status: "denied" });
      },
    );
  }
}
