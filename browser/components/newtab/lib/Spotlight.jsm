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
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeTelemetry.jsm",
  RemoteImages: "resource://activity-stream/lib/RemoteImages.jsm",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.jsm",
});

XPCOMUtils.defineLazyGetter(
  lazy,
  "AWTelemetry",
  () => new lazy.AboutWelcomeTelemetry()
);

const Spotlight = {
  sendUserEventTelemetry(event, message, dispatch) {
    const message_id =
      message.template === "multistage" ? message.content.id : message.id;
    const ping = {
      message_id,
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
    const win = browser.ownerGlobal;
    if (win.gDialogBox.isOpen) {
      return false;
    }
    const spotlight_url = "chrome://browser/content/spotlight.html";

    const dispatchCFRAction =
      // This also blocks CFR impressions, which is fine for current use cases.
      message.content?.metrics === "block" ? () => {} : dispatch;
    let params = { primaryBtn: false, secondaryBtn: false };

    // There are two events named `IMPRESSION` the first one refers to telemetry
    // while the other refers to ASRouter impressions used for the frequency cap
    this.sendUserEventTelemetry("IMPRESSION", message, dispatchCFRAction);
    dispatchCFRAction({ type: "IMPRESSION", data: message });

    const unload = await lazy.RemoteImages.patchMessage(message.content.logo);

    if (message.content?.modal === "tab") {
      let { closedPromise } = win.gBrowser.getTabDialogBox(browser).open(
        spotlight_url,
        {
          features: "resizable=no",
          allowDuplicateDialogs: false,
        },
        [message.content, params]
      );
      await closedPromise;
    } else {
      await win.gDialogBox.open(spotlight_url, [message.content, params]);
    }

    if (unload) {
      unload();
    }

    // If dismissed report telemetry and exit
    if (!params.secondaryBtn && !params.primaryBtn) {
      this.sendUserEventTelemetry("DISMISS", message, dispatchCFRAction);
      return true;
    }

    if (params.secondaryBtn) {
      this.sendUserEventTelemetry("DISMISS", message, dispatchCFRAction);
      lazy.SpecialMessageActions.handleAction(
        message.content.body.secondary.action,
        browser
      );
    }

    if (params.primaryBtn) {
      this.sendUserEventTelemetry("CLICK", message, dispatchCFRAction);
      lazy.SpecialMessageActions.handleAction(
        message.content.body.primary.action,
        browser
      );
    }

    return true;
  },
};

const EXPORTED_SYMBOLS = ["Spotlight"];
