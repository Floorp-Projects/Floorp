/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  pktApi: "chrome://pocket/content/pktApi.sys.mjs",
  SaveToPocket: "chrome://pocket/content/SaveToPocket.sys.mjs",
});

export class AboutPocketParent extends JSWindowActorParent {
  sendResponseMessageToPanel(messageId, payload) {
    this.sendAsyncMessage(`${messageId}_response`, payload);
  }

  isPanalAvailable() {
    return !!this.manager && !this.manager.isClosed;
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "PKT_show_signup": {
        this.browsingContext.topChromeWindow?.pktUI.onShowSignup();
        break;
      }
      case "PKT_show_saved": {
        this.browsingContext.topChromeWindow?.pktUI.onShowSaved();
        break;
      }
      case "PKT_show_home": {
        this.browsingContext.topChromeWindow?.pktUI.onShowHome();
        break;
      }
      case "PKT_close": {
        this.browsingContext.topChromeWindow?.pktUI.closePanel();
        break;
      }
      case "PKT_openTabWithUrl": {
        this.browsingContext.topChromeWindow?.pktUI.onOpenTabWithUrl(
          message.data,
          this.browsingContext.embedderElement.contentPrincipal,
          this.browsingContext.embedderElement.csp
        );
        break;
      }
      case "PKT_openTabWithPocketUrl": {
        this.browsingContext.topChromeWindow?.pktUI.onOpenTabWithPocketUrl(
          message.data,
          this.browsingContext.embedderElement.contentPrincipal,
          this.browsingContext.embedderElement.csp
        );
        break;
      }
      case "PKT_resizePanel": {
        this.browsingContext.topChromeWindow?.pktUI.resizePanel(message.data);
        this.sendResponseMessageToPanel("PKT_resizePanel");
        break;
      }
      case "PKT_getTags": {
        this.sendResponseMessageToPanel("PKT_getTags", lazy.pktApi.getTags());
        break;
      }
      case "PKT_getRecentTags": {
        this.sendResponseMessageToPanel(
          "PKT_getRecentTags",
          lazy.pktApi.getRecentTags()
        );
        break;
      }
      case "PKT_getSuggestedTags": {
        // Ask for suggested tags based on passed url
        const result = await new Promise(resolve => {
          lazy.pktApi.getSuggestedTagsForURL(message.data.url, {
            success: data => {
              var successResponse = {
                status: "success",
                value: {
                  suggestedTags: data.suggested_tags,
                },
              };
              resolve(successResponse);
            },
            error: error => resolve({ status: "error", error }),
          });
        });

        // If the doorhanger is still open, send the result.
        if (this.isPanalAvailable()) {
          this.sendResponseMessageToPanel("PKT_getSuggestedTags", result);
        }
        break;
      }
      case "PKT_addTags": {
        // Pass url and array list of tags, add to existing save item accordingly
        const result = await new Promise(resolve => {
          lazy.pktApi.addTagsToURL(message.data.url, message.data.tags, {
            success: () => resolve({ status: "success" }),
            error: error => resolve({ status: "error", error }),
          });
        });

        // If the doorhanger is still open, send the result.
        if (this.isPanalAvailable()) {
          this.sendResponseMessageToPanel("PKT_addTags", result);
        }
        break;
      }
      case "PKT_deleteItem": {
        // Based on clicking "remove page" CTA, and passed unique item id, remove the item
        const result = await new Promise(resolve => {
          lazy.pktApi.deleteItem(message.data.itemId, {
            success: () => {
              resolve({ status: "success" });
              lazy.SaveToPocket.itemDeleted();
            },
            error: error => resolve({ status: "error", error }),
          });
        });

        // If the doorhanger is still open, send the result.
        if (this.isPanalAvailable()) {
          this.sendResponseMessageToPanel("PKT_deleteItem", result);
        }
        break;
      }
      case "PKT_log": {
        console.log(...Object.values(message.data));
        break;
      }
    }
  }
}
