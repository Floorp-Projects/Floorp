/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class SpeechDispatcherParent extends JSWindowActorParent {
  prefName() {
    return "media.webspeech.synth.dont_notify_on_error";
  }

  disableNotification() {
    Services.prefs.setBoolPref(this.prefName(), true);
  }

  async receiveMessage(aMessage) {
    // The top level browsing context's embedding element should be a xul browser element.
    let browser = this.browsingContext.top.embedderElement;

    if (!browser) {
      // We don't have a browser so bail!
      return;
    }

    let notificationId;

    if (Services.prefs.getBoolPref(this.prefName(), false)) {
      console.info("Opted out from speech-dispatcher error notification");
      return;
    }

    let messageId;
    switch (aMessage.data) {
      case "lib-missing":
        messageId = "speech-dispatcher-lib-missing";
        break;

      case "lib-too-old":
        messageId = "speech-dispatcher-lib-too-old";
        break;

      case "missing-symbol":
        messageId = "speech-dispatcher-missing-symbol";
        break;

      case "open-fail":
        messageId = "speech-dispatcher-open-fail";
        break;

      case "no-voices":
        messageId = "speech-dispatcher-no-voices";
        break;

      default:
        break;
    }

    let MozXULElement = browser.ownerGlobal.MozXULElement;
    MozXULElement.insertFTLIfNeeded("browser/speechDispatcher.ftl");

    // Now actually create the notification
    let notificationBox = browser.getTabBrowser().getNotificationBox(browser);
    if (notificationBox.getNotificationWithValue(notificationId)) {
      return;
    }

    let buttons = [
      {
        supportPage: "speechd-setup",
      },
      {
        "l10n-id": "speech-dispatcher-dismiss-button",
        callback: () => {
          this.disableNotification();
        },
      },
    ];

    let iconURL = "chrome://browser/skin/drm-icon.svg";
    notificationBox.appendNotification(
      notificationId,
      {
        label: { "l10n-id": messageId },
        image: iconURL,
        priority: notificationBox.PRIORITY_INFO_HIGH,
        type: "warning",
      },
      buttons
    );
  }
}
