/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RemoteL10n: "resource://activity-stream/lib/RemoteL10n.jsm",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  AlertsService: ["@mozilla.org/alerts-service;1", "nsIAlertsService"],
});

const ToastNotification = {
  // Allow testing to stub the alerts service.
  get AlertsService() {
    return lazy.AlertsService;
  },

  sendUserEventTelemetry(event, message, dispatch) {
    const ping = {
      message_id: message.id,
      event,
    };
    dispatch({
      type: "TOAST_NOTIFICATION_TELEMETRY",
      data: { action: "toast_notification_user_event", ...ping },
    });
  },

  /**
   * Show a toast notification.
   * @param message             Message containing content to show.
   * @param dispatch            A function to dispatch resulting actions.
   * @return                    boolean value capturing if toast notification was displayed.
   */
  async showToastNotification(message, dispatch) {
    let { content } = message;
    let title = await lazy.RemoteL10n.formatLocalizableText(content.title);
    let body = await lazy.RemoteL10n.formatLocalizableText(content.body);

    // There are two events named `IMPRESSION` the first one refers to telemetry
    // while the other refers to ASRouter impressions used for the frequency cap
    this.sendUserEventTelemetry("IMPRESSION", message, dispatch);
    dispatch({ type: "IMPRESSION", data: message });

    let alert = Cc["@mozilla.org/alert-notification;1"].createInstance(
      Ci.nsIAlertNotification
    );
    let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    alert.init(
      content.tag,
      content.image_url
        ? Services.urlFormatter.formatURL(content.image_url)
        : content.image_url,
      title,
      body,
      true /* aTextClickable */,
      content.data,
      null /* aDir */,
      null /* aLang */,
      null /* aData */,
      systemPrincipal,
      null /* aInPrivateBrowsing */,
      content.requireInteraction
    );

    if (content.actions) {
      let actions = Cu.cloneInto(content.actions, {});
      for (let action of actions) {
        if (action.title) {
          action.title = await lazy.RemoteL10n.formatLocalizableText(
            action.title
          );
        }
      }
      alert.actions = actions;
    }

    if (content.launch_url) {
      alert.launchURL = Services.urlFormatter.formatURL(content.launch_url);
    }

    this.AlertsService.showAlert(alert);

    return true;
  },
};

const EXPORTED_SYMBOLS = ["ToastNotification"];
