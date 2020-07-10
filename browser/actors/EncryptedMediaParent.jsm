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

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
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

  getEMEDisabledFragment(aBrowser) {
    let mainMessage = gNavigatorBundle.GetStringFromName(
      "emeNotifications.drmContentDisabled.message"
    );
    let text = gNavigatorBundle.GetStringFromName(
      "emeNotifications.drmContentDisabled.learnMoreLabel"
    );
    let document = aBrowser.ownerDocument;
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    let link = document.createXULElement("label", { is: "text-link" });
    link.setAttribute("href", baseURL + "drm-content");
    link.textContent = text;
    return BrowserUtils.getLocalizedFragment(document, mainMessage, link);
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
        notificationMessage = this.getEMEDisabledFragment(browser);
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
    if (buttonCallback) {
      let msgPrefix = "emeNotifications." + notificationId + ".";
      let btnLabelId = msgPrefix + "button.label";
      let btnAccessKeyId = msgPrefix + "button.accesskey";
      buttons.push({
        label: gNavigatorBundle.GetStringFromName(btnLabelId),
        accessKey: gNavigatorBundle.GetStringFromName(btnAccessKeyId),
        callback: buttonCallback,
      });
    }

    let iconURL = "chrome://browser/skin/drm-icon.svg";
    notificationBox.appendNotification(
      notificationMessage,
      notificationId,
      iconURL,
      notificationBox.PRIORITY_WARNING_MEDIUM,
      buttons
    );
  }

  showPopupNotificationForSuccess(aBrowser) {
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

    let msgPrefix = "emeNotifications.drmContentPlaying.";
    let msgId = msgPrefix + "message2";
    let btnLabelId = msgPrefix + "button.label";
    let btnAccessKeyId = msgPrefix + "button.accesskey";

    let message = gNavigatorBundle.formatStringFromName(msgId, [
      gBrandBundle.GetStringFromName("brandShortName"),
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
      label: gNavigatorBundle.GetStringFromName(btnLabelId),
      accessKey: gNavigatorBundle.GetStringFromName(btnAccessKeyId),
      callback() {
        aBrowser.ownerGlobal.openPreferences("general-drm");
      },
      dismiss: true,
    };
    let options = {
      dismissed: true,
      eventCallback: aTopic => aTopic == "swapping",
      learnMoreURL:
        Services.urlFormatter.formatURLPref("app.support.baseURL") +
        "drm-content",
    };
    aBrowser.ownerGlobal.PopupNotifications.show(
      aBrowser,
      "drmContentPlaying",
      message,
      anchorId,
      mainAction,
      null,
      options
    );
  }
}
