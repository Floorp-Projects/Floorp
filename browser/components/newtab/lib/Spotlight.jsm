/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutWelcomeTelemetry:
    "resource:///modules/aboutwelcome/AboutWelcomeTelemetry.jsm",
});

ChromeUtils.defineLazyGetter(
  lazy,
  "AWTelemetry",
  () => new lazy.AboutWelcomeTelemetry()
);

const Spotlight = {
  sendUserEventTelemetry(event, message, dispatch) {
    const ping = {
      message_id: message.content.id,
      event,
    };
    dispatch({
      type: "SPOTLIGHT_TELEMETRY",
      data: { action: "spotlight_user_event", ...ping },
    });
  },

  defaultDispatch(message) {
    if (message.type === "SPOTLIGHT_TELEMETRY") {
      const { message_id, event } = message.data;
      lazy.AWTelemetry.sendTelemetry({ message_id, event });
    }
  },

  /**
   * Shows spotlight tab or window modal specific to the given browser
   * @param browser             The browser for spotlight display
   * @param message             Message containing content to show
   * @param dispatchCFRAction   A function to dispatch resulting actions
   * @return                    boolean value capturing if spotlight was displayed
   */
  async showSpotlightDialog(browser, message, dispatch = this.defaultDispatch) {
    const win = browser?.ownerGlobal;
    if (!win || win.gDialogBox.isOpen) {
      return false;
    }
    const spotlight_url = "chrome://browser/content/spotlight.html";

    const dispatchCFRAction =
      // This also blocks CFR impressions, which is fine for current use cases.
      message.content?.metrics === "block" ? () => {} : dispatch;

    // This handles `IMPRESSION` events used by ASRouter for frequency caps.
    // AboutWelcome handles `IMPRESSION` events for telemetry.
    this.sendUserEventTelemetry("IMPRESSION", message, dispatchCFRAction);
    dispatchCFRAction({ type: "IMPRESSION", data: message });

    if (message.content?.modal === "tab") {
      let { closedPromise } = win.gBrowser.getTabDialogBox(browser).open(
        spotlight_url,
        {
          features: "resizable=no",
          allowDuplicateDialogs: false,
        },
        message.content
      );
      await closedPromise;
    } else {
      await win.gDialogBox.open(spotlight_url, message.content);
    }

    // If dismissed report telemetry and exit
    this.sendUserEventTelemetry("DISMISS", message, dispatchCFRAction);
    return true;
  },
};

const EXPORTED_SYMBOLS = ["Spotlight"];
