/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  isWebPanelBrowserId,
  isWebPanelClipboardReadKind,
  NR_WEB_PANEL_CLIPBOARD_READ_MESSAGE,
  type NRWebPanelClipboardEntry,
  type NRWebPanelClipboardReadResult,
  WEB_PANEL_BROWSER_IDS_SHARED_KEY,
  WEB_PANEL_CLIPBOARD_FLAVORS,
} from "../common/NRWebPanelClipboardTypes.ts";

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs",
);

interface FocusChainElement {
  readonly ownerDocument: FocusChainDocument | null;
}

interface FocusChainDocument {
  readonly activeElement: unknown;
  hasFocus(): boolean;
  readonly defaultView: {
    readonly browsingContext?: {
      readonly embedderElement?: FocusChainElement | null;
    } | null;
  } | null;
}

/**
 * Gecko's nsBaseClipboard::RequestUserConfirmation() requires the content to
 * be in the focused tab of the focused window. For a web panel the content
 * <browser> is nested one chrome document deeper, so require every embedder
 * on the way up to be the active element of its document, and the topmost
 * chrome document to have focus.
 */
export function isFocusedNestedBrowser(
  browser: FocusChainElement | null | undefined,
): boolean {
  let element = browser ?? null;
  let depth = 0;
  while (element) {
    const doc = element.ownerDocument;
    if (!doc || doc.activeElement !== element) {
      return false;
    }
    const embedder = doc.defaultView?.browsingContext?.embedderElement ?? null;
    if (!embedder) {
      return depth > 0 && doc.hasFocus();
    }
    element = embedder;
    depth++;
  }
  return false;
}

function readTransferData(
  snapshot: nsIClipboardDataSnapshot,
  flavor: string,
): NRWebPanelClipboardEntry | null {
  const transferable = Cc["@mozilla.org/widget/transferable;1"]
    .createInstance(Ci.nsITransferable);
  // A null load context is what Gecko uses for clipboard reads.
  transferable.init(null as unknown as nsILoadContext);
  transferable.addDataFlavor(flavor);
  snapshot.getDataSync(transferable);

  const data: { value?: nsISupports } = {};
  transferable.getTransferData(flavor, data);
  let value = data.value;
  if (value instanceof Ci.nsISupportsString) {
    return { type: flavor, text: value.data };
  }
  // Some platforms hand image flavors over as a decoded image.
  if (value instanceof Ci.imgIContainer) {
    value = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .encodeImage(value, flavor);
  }
  if (value instanceof Ci.nsIInputStream) {
    return { type: flavor, bytes: readAllBytes(value) };
  }
  return null;
}

/** available() can report less than the whole stream, so read until empty. */
function readAllBytes(stream: nsIInputStream): ArrayBuffer {
  const chunks: Uint8Array[] = [];
  let total = 0;
  try {
    for (let size = stream.available(); size > 0; size = stream.available()) {
      const chunk = new Uint8Array(
        NetUtil.readInputStream(stream, size) as ArrayBuffer,
      );
      chunks.push(chunk);
      total += chunk.byteLength;
    }
  } catch (error) {
    // A closed stream just means everything has been read.
    if ((error as { result?: number }).result !== Cr.NS_BASE_STREAM_CLOSED) {
      throw error;
    }
  }
  const bytes = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    bytes.set(chunk, offset);
    offset += chunk.byteLength;
  }
  return bytes.buffer;
}

/** Reads the requested flavors in Gecko's order, skipping missing ones. */
function readClipboardEntries(
  flavors: readonly string[],
  windowGlobal: WindowGlobalParent,
): NRWebPanelClipboardEntry[] {
  const snapshot = Services.clipboard.getDataSnapshotSync(
    [...flavors],
    Ci.nsIClipboard.kGlobalClipboard,
    // WindowGlobalParent is a WindowContext; the typings disagree only on
    // the nullability of browsingContext.currentURI.
    windowGlobal as unknown as WindowContext,
  );
  const available = snapshot.flavorList;
  const entries: NRWebPanelClipboardEntry[] = [];
  for (const flavor of flavors) {
    if (!available.includes(flavor)) {
      continue;
    }
    try {
      const entry = readTransferData(snapshot, flavor);
      if (entry) {
        entries.push(entry);
      }
    } catch (error) {
      console.error("[NRWebPanelClipboard]", flavor, error);
    }
  }
  return entries;
}

export class NRWebPanelClipboardParent extends JSWindowActorParent {
  async receiveMessage(
    message: { name: string; data?: { kind?: unknown } },
  ): Promise<NRWebPanelClipboardReadResult | undefined> {
    if (message.name !== NR_WEB_PANEL_CLIPBOARD_READ_MESSAGE) {
      return undefined;
    }

    const kind = message.data?.kind;
    const windowContext = this.manager;
    const browser = this.browsingContext?.top?.embedderElement ?? null;
    if (
      !isWebPanelClipboardReadKind(kind) ||
      !windowContext ||
      !browser ||
      !isWebPanelBrowserId(
        Services.ppmm.sharedData.get(WEB_PANEL_BROWSER_IDS_SHARED_KEY),
        (browser as XULBrowserElement).browserId,
      ) ||
      !isFocusedNestedBrowser(browser as unknown as FocusChainElement)
    ) {
      return { status: "denied" };
    }

    try {
      const result = await Services.prompt.confirmUserPaste(windowContext);
      if (!result.getProperty("ok")) {
        return { status: "denied" };
      }
      return {
        status: "ok",
        entries: readClipboardEntries(
          WEB_PANEL_CLIPBOARD_FLAVORS[kind],
          windowContext,
        ),
      };
    } catch (error) {
      // confirmUserPaste() rejects while another paste menu is open.
      console.error("[NRWebPanelClipboard]", error);
      return { status: "denied" };
    }
  }
}
