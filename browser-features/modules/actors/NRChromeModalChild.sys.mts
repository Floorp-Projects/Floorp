/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { TForm } from "../../chrome/common/modal-parent/utils/type.ts";
export class NRChromeModalChild extends JSWindowActorChild {
  actorCreated() {
    console.log("NRChromeModalChild actor created");
  }

  async receiveMessage(message: ReceiveMessageArgument) {
    const window = this.contentWindow as Window;
    switch (message.name) {
      case "NRChromeModal:show": {
        await this.waitForReady(window);
        this.renderContent(window, message.data);
        return await this.waitForUserInput(window);
      }
    }
    return null;
  }

  private waitForReady(win: Window): Promise<void> {
    return new Promise((resolve) => {
      const doc = win.document as Document & { documentElement: HTMLElement };
      if (doc?.documentElement?.dataset?.noraModalReady === "true") {
        resolve();
        return;
      }

      let count = 0;
      const interval = win.setInterval(() => {
        const currentDoc = win.document as Document & {
          documentElement: HTMLElement;
        };
        if (currentDoc?.documentElement?.dataset?.noraModalReady === "true") {
          win.clearInterval(interval);
          resolve();
        } else if (count > 50) {
          win.clearInterval(interval);
          console.error(
            "NRChromeModalChild: Timeout waiting for nora-modal-ready",
          );
          resolve();
        }
        count++;
      }, 100);
    });
  }

  renderContent(win: Window, from: TForm) {
    // Use postMessage instead of CustomEvent to bypass strict security wrapper issues
    // postMessage handles structured cloning automatically across boundaries
    try {
      const detail = JSON.stringify(from);

      // Post message to the window
      // We use "*" as targetOrigin because we are in a restricted modal environment
      // and we want to ensure the message reaches the content
      win.postMessage({ type: "nora-modal-init", detail }, "*");
    } catch (e) {
      console.error("NRChromeModalChild: Error posting message", e);
    }
  }

  private waitForUserInput(win: Window): Promise<Record<string, unknown>> {
    return new Promise((resolve) => {
      // Listen for messages from the content window
      // We need to listen on the window itself because postMessage targets the window
      // But in actor context, we might need to listen to the message manager or add listener to the window

      const messageHandler = (event: MessageEvent) => {
        // Verify the message is from our content
        // In actor, event.source might be a proxy or wrapper, strict equality check might fail
        // So we rely on the message structure
        if (event.data && event.data.type === "nora-modal-submit") {
          win.removeEventListener("message", messageHandler);

          const data = event.data.detail;
          resolve(data);
          this.removeContent(win);
        }
      };

      // Add listener to the content window
      // Note: In JSWindowActorChild, we are in the same process but separated by security wrapper
      // Listening on 'win' should catch postMessage from content to itself
      win.addEventListener("message", messageHandler);

      // Fallback: also support legacy sendForm injection for safety or if postMessage fails
      const originalSendForm = win.sendForm;
      win.sendForm = (data: Record<string, unknown>) => {
        win.removeEventListener("message", messageHandler);
        resolve(data);
        if (originalSendForm) {
          return originalSendForm.call(win, data);
        }
        this.removeContent(win);
        return null;
      };
    });
  }

  private removeContent(win: Window) {
    // Use postMessage to trigger cleanup in the content
    try {
      win.postMessage({ type: "nora-modal-remove" }, "*");
    } catch (e) {
      console.error("NRChromeModalChild: Error removing content", e);
    }

    // Legacy fallback
    if (typeof win.removeForm === "function") {
      win.removeForm();
    }
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
