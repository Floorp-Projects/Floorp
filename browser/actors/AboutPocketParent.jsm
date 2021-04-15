/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutPocketParent"];
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "pktApi",
  "chrome://pocket/content/pktApi.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGetter(this, "gPocketBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/pocket.properties"
  );
});

class AboutPocketParent extends JSWindowActorParent {
  sendResponseMessageToPanel(messageId, panelId, payload) {
    this.sendAsyncMessage(`${messageId}_response_${panelId}`, payload);
  }

  isPanalAvailable() {
    return !!this.manager && !this.manager.isClosed;
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "PKT_initL10N": {
        var strings = {};
        for (let str of gPocketBundle.getSimpleEnumeration()) {
          if (str.key in message.data.payload) {
            strings[str.key] = gPocketBundle.formatStringFromName(
              str.key,
              message.data.payload[str.key]
            );
          } else {
            strings[str.key] = str.value;
          }
        }

        this.sendResponseMessageToPanel("PKT_initL10N", message.data.panelId, {
          strings,
          dir: Services.locale.isAppLocaleRTL ? "rtl" : "ltr",
        });
        break;
      }
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
          message.data.panelId,
          message.data.payload,
          this.browsingContext.embedderElement.contentDocument.nodePrincipal,
          this.browsingContext.embedderElement.contentDocument.csp
        );
        break;
      }
      case "PKT_openTabWithPocketUrl": {
        this.browsingContext.topChromeWindow?.pktUI.onOpenTabWithPocketUrl(
          message.data.panelId,
          message.data.payload,
          this.browsingContext.embedderElement.contentDocument.nodePrincipal,
          this.browsingContext.embedderElement.contentDocument.csp
        );
        break;
      }
      case "PKT_resizePanel": {
        this.browsingContext.topChromeWindow?.pktUI.resizePanel(
          message.data.payload
        );
        this.sendResponseMessageToPanel(
          "PKT_resizePanel",
          message.data.panelId
        );
        break;
      }
      case "PKT_getTags": {
        this.sendResponseMessageToPanel(
          "PKT_getTags",
          message.data.panelId,
          pktApi.getTags()
        );
        break;
      }
      case "PKT_getSuggestedTags": {
        // Ask for suggested tags based on passed url
        const result = await new Promise(resolve => {
          pktApi.getSuggestedTagsForURL(message.data.payload.url, {
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
          this.sendResponseMessageToPanel(
            "PKT_getSuggestedTags",
            message.data.panelId,
            result
          );
        }
        break;
      }
      case "PKT_addTags": {
        // Pass url and array list of tags, add to existing save item accordingly
        const result = await new Promise(resolve => {
          pktApi.addTagsToURL(
            message.data.payload.url,
            message.data.payload.tags,
            {
              success: () => resolve({ status: "success" }),
              error: error => resolve({ status: "error", error }),
            }
          );
        });

        // If the doorhanger is still open, send the result.
        if (this.isPanalAvailable()) {
          this.sendResponseMessageToPanel(
            "PKT_addTags",
            message.data.panelId,
            result
          );
        }
        break;
      }
      case "PKT_deleteItem": {
        // Based on clicking "remove page" CTA, and passed unique item id, remove the item
        const result = await new Promise(resolve => {
          pktApi.deleteItem(message.data.payload.itemId, {
            success: () => {
              resolve({ status: "success" });
              this.browsingContext.topChromeWindow?.pktUI
                .getPanelFrame()
                .setAttribute("itemAdded", "false");
            },
            error: error => resolve({ status: "error", error }),
          });
        });

        // If the doorhanger is still open, send the result.
        if (this.isPanalAvailable()) {
          this.sendResponseMessageToPanel(
            "PKT_deleteItem",
            message.data.panelId,
            result
          );
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
