/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["EncryptedMediaParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gFluentStrings", function() {
  return new Localization(["branding/brand.ftl", "browser/browser.ftl"], true);
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gProtonEnabled",
  "browser.proton.enabled",
  false
);

class EncryptedMediaParent extends JSWindowActorParent {
  isUiEnabled() {
    return Services.prefs.getBoolPref("browser.eme.ui.enabled");
  }

  ensureEMEEnabled(aBrowser, aKeySystem) {
    Services.prefs.setBoolPref("media.eme.enabled", true);
    if (
      aKeySystem &&
      aKeySystem == "com.widevine.alpha" &&
      Services.prefs.getPrefType("media.gmp-widevinecdm.enabled") &&
      !Services.prefs.getBoolPref("media.gmp-widevinecdm.enabled")
    ) {
      Services.prefs.setBoolPref("media.gmp-widevinecdm.enabled", true);
    }
    aBrowser.reload();
  }

  isKeySystemVisible(aKeySystem) {
    if (!aKeySystem) {
      return false;
    }
    if (
      aKeySystem == "com.widevine.alpha" &&
      Services.prefs.getPrefType("media.gmp-widevinecdm.visible")
    ) {
      return Services.prefs.getBoolPref("media.gmp-widevinecdm.visible");
    }
    return true;
  }

  getMessageWithBrandName(aNotificationId) {
    let msgId = "emeNotifications." + aNotificationId + ".message";
    return gNavigatorBundle.formatStringFromName(msgId, [
      gBrandBundle.GetStringFromName("brandShortName"),
    ]);
  }

  receiveMessage(aMessage) {
    // The top level browsing context's embedding element should be a xul browser element.
    let browser = this.browsingContext.top.embedderElement;

    if (!browser) {
      // We don't have a browser so bail!
      return;
    }

    let parsedData;
    try {
      parsedData = JSON.parse(aMessage.data);
    } catch (ex) {
      Cu.reportError("Malformed EME video message with data: " + aMessage.data);
      return;
    }
    let { status, keySystem } = parsedData;

    // First, see if we need to do updates. We don't need to do anything for
    // hidden keysystems:
    if (!this.isKeySystemVisible(keySystem)) {
      return;
    }
    if (status == "cdm-not-installed") {
      Services.obs.notifyObservers(browser, "EMEVideo:CDMMissing");
    }

    // Don't need to show UI if disabled.
    if (!this.isUiEnabled()) {
      return;
    }

    let notificationId;
    let buttonCallback;
    let supportPage;
    // Notification message can be either a string or a DOM fragment.
    let notificationMessage;
    switch (status) {
      case "available":
      case "cdm-created":
        // Only show the chain icon for proprietary CDMs. Clearkey is not one.
        if (keySystem != "org.w3.clearkey") {
          this.showPopupNotificationForSuccess(browser, keySystem);
        }
        // ... and bail!
        return;

      case "api-disabled":
      case "cdm-disabled":
        notificationId = "drmContentDisabled";
        buttonCallback = () => {
          this.ensureEMEEnabled(browser, keySystem);
        };
        notificationMessage = gNavigatorBundle.GetStringFromName(
          "emeNotifications.drmContentDisabled.message2"
        );
        supportPage = "drm-content";
        break;

      case "cdm-not-installed":
        notificationId = "drmContentCDMInstalling";
        notificationMessage = this.getMessageWithBrandName(notificationId);
        break;

      case "cdm-not-supported":
        // Not to pop up user-level notification because they cannot do anything
        // about it.
        return;
      default:
        Cu.reportError(
          new Error(
            "Unknown message ('" +
              status +
              "') dealing with EME key request: " +
              aMessage.data
          )
        );
        return;
    }

    // Now actually create the notification

    let notificationBox = browser.getTabBrowser().getNotificationBox(browser);
    if (notificationBox.getNotificationWithValue(notificationId)) {
      return;
    }

    let buttons = [];
    if (supportPage) {
      buttons.push({ supportPage });
    }
    if (buttonCallback) {
      let msgPrefix = "emeNotifications." + notificationId + ".";
      let manageLabelId = msgPrefix + "button.label";
      let manageAccessKeyId = msgPrefix + "button.accesskey";
      buttons.push({
        label: gNavigatorBundle.GetStringFromName(manageLabelId),
        accessKey: gNavigatorBundle.GetStringFromName(manageAccessKeyId),
        callback: buttonCallback,
      });
    }

    let iconURL = "chrome://browser/skin/drm-icon.svg";
    notificationBox.appendNotification(
      notificationMessage,
      notificationId,
      iconURL,
      notificationBox.PRIORITY_INFO_HIGH,
      buttons
    );
  }

  async showPopupNotificationForSuccess(aBrowser) {
    // We're playing EME content! Remove any "we can't play because..." messages.
    let notificationBox = aBrowser.getTabBrowser().getNotificationBox(aBrowser);
    ["drmContentDisabled", "drmContentCDMInstalling"].forEach(function(value) {
      let notification = notificationBox.getNotificationWithValue(value);
      if (notification) {
        notificationBox.removeNotification(notification);
      }
    });

    // Don't bother creating it if it's already there:
    if (
      aBrowser.ownerGlobal.PopupNotifications.getNotification(
        "drmContentPlaying",
        aBrowser
      )
    ) {
      return;
    }

    let msgPrefix = "eme-notifications-drm-content-playing";
    let msgId = msgPrefix;
    let manageLabelId = msgPrefix + "-manage";
    let manageAccessKeyId = msgPrefix + "-manage-accesskey";
    let dismissLabelId = msgPrefix + "-dismiss";
    let dismissAccessKeyId = msgPrefix + "-dismiss-accesskey";

    let [
      message,
      manageLabel,
      manageAccessKey,
      dismissLabel,
      dismissAccessKey,
    ] = await gFluentStrings.formatValues([
      msgId,
      manageLabelId,
      manageAccessKeyId,
      dismissLabelId,
      dismissAccessKeyId,
    ]);

    let anchorId = "eme-notification-icon";
    let firstPlayPref = "browser.eme.ui.firstContentShown";
    let document = aBrowser.ownerDocument;
    if (
      !Services.prefs.getPrefType(firstPlayPref) ||
      !Services.prefs.getBoolPref(firstPlayPref)
    ) {
      document.getElementById(anchorId).setAttribute("firstplay", "true");
      Services.prefs.setBoolPref(firstPlayPref, true);
    } else {
      document.getElementById(anchorId).removeAttribute("firstplay");
    }

    let mainAction = {
      label: manageLabel,
      accessKey: manageAccessKey,
      callback() {
        aBrowser.ownerGlobal.openPreferences("general-drm");
      },
      dismiss: true,
      disableHighlight: gProtonEnabled,
    };

    let secondaryActions = [
      {
        label: dismissLabel,
        accessKey: dismissAccessKey,
        callback: () => {},
        dismiss: true,
      },
    ];

    let options = {
      dismissed: true,
      eventCallback: aTopic => aTopic == "swapping",
      learnMoreURL:
        Services.urlFormatter.formatURLPref("app.support.baseURL") +
        "drm-content",
      hideClose: true,
    };
    aBrowser.ownerGlobal.PopupNotifications.show(
      aBrowser,
      "drmContentPlaying",
      message,
      anchorId,
      mainAction,
      secondaryActions,
      options
    );
  }
}
