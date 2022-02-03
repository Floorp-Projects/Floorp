/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.jsm",
});

const Spotlight = {
  sendUserEventTelemetry(event, message, dispatch) {
    const ping = {
      message_id: message.id,
      event,
    };
    dispatch({
      type: "SPOTLIGHT_TELEMETRY",
      data: { action: "spotlight_user_event", ...ping },
    });
  },

  async showSpotlightDialog(browser, message, dispatchCFRAction) {
    const win = browser.ownerGlobal;
    if (win.gDialogBox.isOpen) {
      return false;
    }

    let params = { primaryBtn: false, secondaryBtn: false };

    // There are two events named `IMPRESSION` the first one refers to telemetry
    // while the other refers to ASRouter impressions used for the frequency cap
    this.sendUserEventTelemetry("IMPRESSION", message, dispatchCFRAction);
    dispatchCFRAction({ type: "IMPRESSION", data: message });

    await win.gDialogBox.open("chrome://browser/content/spotlight.html", [
      message.content,
      params,
    ]);

    // If dismissed report telemetry and exit
    if (!params.secondaryBtn && !params.primaryBtn) {
      this.sendUserEventTelemetry("DISMISS", message, dispatchCFRAction);
      return true;
    }

    if (params.secondaryBtn) {
      this.sendUserEventTelemetry("DISMISS", message, dispatchCFRAction);
      SpecialMessageActions.handleAction(
        message.content.body.secondary.action,
        browser
      );
    }

    if (params.primaryBtn) {
      this.sendUserEventTelemetry("CLICK", message, dispatchCFRAction);
      SpecialMessageActions.handleAction(
        message.content.body.primary.action,
        browser
      );
    }

    return true;
  },
};

this.Spotlight = Spotlight;

const EXPORTED_SYMBOLS = ["Spotlight"];
