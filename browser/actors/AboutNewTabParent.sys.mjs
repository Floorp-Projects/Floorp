/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
});

// A mapping of loaded new tab pages, where the mapping is:
//   browser -> { actor, browser, browsingContext, portID, url, loaded }
let gLoadedTabs = new Map();

export class AboutNewTabParent extends JSWindowActorParent {
  static get loadedTabs() {
    return gLoadedTabs;
  }

  getTabDetails() {
    let browser = this.browsingContext.top.embedderElement;
    return browser ? gLoadedTabs.get(browser) : null;
  }

  handleEvent(event) {
    if (event.type == "SwapDocShells") {
      let oldBrowser = this.browsingContext.top.embedderElement;
      let newBrowser = event.detail;

      let tabDetails = gLoadedTabs.get(oldBrowser);
      if (tabDetails) {
        tabDetails.browser = newBrowser;
        gLoadedTabs.delete(oldBrowser);
        gLoadedTabs.set(newBrowser, tabDetails);

        oldBrowser.removeEventListener("SwapDocShells", this);
        newBrowser.addEventListener("SwapDocShells", this);
      }
    }
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "AboutNewTabVisible":
        await lazy.ASRouter.waitForInitialized;
        lazy.ASRouter.sendTriggerMessage({
          browser: this.browsingContext.top.embedderElement,
          // triggerId and triggerContext
          id: "defaultBrowserCheck",
          context: { source: "newtab" },
        });
        break;

      case "Init": {
        let actor = message.target;
        let browsingContext = actor.browsingContext;
        let browser = browsingContext.top.embedderElement;
        if (!browser) {
          return;
        }

        let tabDetails = {
          actor,
          browser,
          browsingContext,
          portID: message.data.portID,
          url: message.data.url,
        };
        gLoadedTabs.set(browser, tabDetails);

        browser.addEventListener("SwapDocShells", this);
        browser.addEventListener("EndSwapDocShells", this);

        this.notifyActivityStreamChannel("onNewTabInit", message, tabDetails);
        break;
      }

      case "Load":
        this.notifyActivityStreamChannel("onNewTabLoad", message);
        break;

      case "Unload": {
        let tabDetails = this.getTabDetails();
        if (!tabDetails) {
          // When closing a tab, the embedderElement can already be disconnected, so
          // an a backup, look up the tab details by browsing context.
          tabDetails = this.getByBrowsingContext(
            message.target.browsingContext
          );
        }

        if (!tabDetails) {
          return;
        }

        tabDetails.browser.removeEventListener("EndSwapDocShells", this);

        gLoadedTabs.delete(tabDetails.browser);

        this.notifyActivityStreamChannel("onNewTabUnload", message, tabDetails);
        break;
      }

      case "ActivityStream:ContentToMain":
        this.notifyActivityStreamChannel("onMessage", message);
        break;
    }
  }

  notifyActivityStreamChannel(name, message, tabDetails) {
    if (!tabDetails) {
      tabDetails = this.getTabDetails();
      if (!tabDetails) {
        return;
      }
    }

    let channel = this.getChannel();
    if (!channel) {
      return;
    }

    let messageToSend = {
      target: message.target,
      data: message.data || {},
    };

    channel[name](messageToSend, tabDetails);
  }

  getByBrowsingContext(expectedBrowsingContext) {
    for (let tabDetails of AboutNewTabParent.loadedTabs.values()) {
      if (tabDetails.browsingContext === expectedBrowsingContext) {
        return tabDetails;
      }
    }

    return null;
  }

  getChannel() {
    return lazy.AboutNewTab.activityStream?.store?.getMessageChannel();
  }
}
