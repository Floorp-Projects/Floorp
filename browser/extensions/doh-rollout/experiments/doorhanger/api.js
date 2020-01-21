/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global BrowserWindowTracker, ExtensionCommon, ExtensionAPI */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

var { EventManager, EventEmitter } = ExtensionCommon;
const {
  Management: {
    global: { tabTracker },
  },
} = ChromeUtils.import("resource://gre/modules/Extension.jsm", null);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);

// Return most recent NON-PRIVATE browser window, so that we can
// manipulate chrome elements on it.
function getMostRecentBrowserWindow() {
  return BrowserWindowTracker.getTopWindow({
    private: false,
    allowPopups: false,
  });
}

class DoorhangerEventEmitter extends EventEmitter {
  async emitShow({
    name,
    text,
    okLabel,
    okAccessKey,
    cancelLabel,
    cancelAccessKey,
  }) {
    const self = this;
    const recentWindow = getMostRecentBrowserWindow();
    const browser = recentWindow.gBrowser.selectedBrowser;
    const tabId = tabTracker.getBrowserTabId(browser);

    const primaryAction = {
      disableHighlight: false,
      label: okLabel,
      accessKey: okAccessKey,
      callback: () => {
        self.emit("doorhanger-accept", tabId);
      },
    };
    const secondaryActions = [
      {
        label: cancelLabel,
        accessKey: cancelAccessKey,
        callback: () => {
          self.emit("doorhanger-decline", tabId);
        },
      },
    ];

    let learnMoreURL = Services.urlFormatter.formatURL(
      "https://support.mozilla.org/%LOCALE%/kb/firefox-dns-over-https"
    );

    const options = {
      hideClose: true,
      persistWhileVisible: true,
      persistent: true,
      autofocus: true,
      name,
      popupIconURL: "chrome://browser/skin/connection-secure.svg",
      learnMoreURL,
      escAction: "buttoncommand",
      removeOnDismissal: false,
    };

    recentWindow.PopupNotifications.show(
      browser,
      "doh-first-time",
      text,
      null,
      primaryAction,
      secondaryActions,
      options
    );
  }
}

this.doorhanger = class doorhanger extends ExtensionAPI {
  getAPI(context) {
    const doorhangerEventEmitter = new DoorhangerEventEmitter();
    return {
      experiments: {
        doorhanger: {
          async show(properties) {
            let profileAge = await ChromeUtils.import(
              "resource://gre/modules/ProfileAge.jsm",
              {}
            ).ProfileAge();

            let creationDate = await profileAge.created;
            let firstUse = await profileAge.firstUse;
            let resetDate = await profileAge.reset;
            let profileDate = resetDate || firstUse || creationDate;

            // We only need to show the doorhanger to users who have not been
            // shown a version of the privacy statement that includes DoH at
            // first-run.
            let profileCreationThreshold = parseInt(
              Preferences.get("doh-rollout.profileCreationThreshold")
            );
            if (
              !isNaN(profileCreationThreshold) &&
              profileDate > profileCreationThreshold
            ) {
              return false;
            }

            await doorhangerEventEmitter.emitShow(properties);
            return true;
          },
          onDoorhangerAccept: new EventManager({
            context,
            name: "doorhanger.onDoorhangerAccept",
            register: fire => {
              let listener = (value, tabId) => {
                fire.async(tabId);
              };
              doorhangerEventEmitter.on("doorhanger-accept", listener);
              return () => {
                doorhangerEventEmitter.off("doorhanger-accept", listener);
              };
            },
          }).api(),
          onDoorhangerDecline: new EventManager({
            context,
            name: "doorhanger.onDoorhangerDecline",
            register: fire => {
              let listener = (value, tabId) => {
                fire.async(tabId);
              };
              doorhangerEventEmitter.on("doorhanger-decline", listener);
              return () => {
                doorhangerEventEmitter.off("doorhanger-decline", listener);
              };
            },
          }).api(),
        },
      },
    };
  }
};
