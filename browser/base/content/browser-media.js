/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var gEMEHandler = {
  get uiEnabled() {
    let emeUIEnabled = Services.prefs.getBoolPref("browser.eme.ui.enabled");
    // Force-disable on WinXP:
    if (navigator.platform.toLowerCase().startsWith("win")) {
      emeUIEnabled = emeUIEnabled && parseFloat(Services.sysinfo.get("version")) >= 6;
    }
    return emeUIEnabled;
  },
  ensureEMEEnabled(browser, keySystem) {
    Services.prefs.setBoolPref("media.eme.enabled", true);
    if (keySystem &&
        keySystem == "com.widevine.alpha" &&
        Services.prefs.getPrefType("media.gmp-widevinecdm.enabled") &&
        !Services.prefs.getBoolPref("media.gmp-widevinecdm.enabled")) {
      Services.prefs.setBoolPref("media.gmp-widevinecdm.enabled", true);
    }
    browser.reload();
  },
  isKeySystemVisible(keySystem) {
    if (!keySystem) {
      return false;
    }
    if (keySystem == "com.widevine.alpha" &&
        Services.prefs.getPrefType("media.gmp-widevinecdm.visible")) {
      return Services.prefs.getBoolPref("media.gmp-widevinecdm.visible");
    }
    return true;
  },
  getLearnMoreLink(msgId) {
    let text = gNavigatorBundle.getString("emeNotifications." + msgId + ".learnMoreLabel");
    let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
    return "<label class='text-link' href='" + baseURL + "drm-content'>" +
           text + "</label>";
  },
  receiveMessage({target: browser, data: data}) {
    let parsedData;
    try {
      parsedData = JSON.parse(data);
    } catch (ex) {
      Cu.reportError("Malformed EME video message with data: " + data);
      return;
    }
    let {status: status, keySystem: keySystem} = parsedData;
    // Don't need to show if disabled or keysystem not visible.
    if (!this.uiEnabled || !this.isKeySystemVisible(keySystem)) {
      return;
    }

    let notificationId;
    let buttonCallback;
    let params = [];
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
        buttonCallback = gEMEHandler.ensureEMEEnabled.bind(gEMEHandler, browser, keySystem)
        params = [this.getLearnMoreLink(notificationId)];
        break;

      case "cdm-insufficient-version":
        notificationId = "drmContentCDMInsufficientVersion";
        params = [this._brandShortName];
        break;

      case "cdm-not-installed":
        notificationId = "drmContentCDMInstalling";
        params = [this._brandShortName];
        break;

      case "cdm-not-supported":
        // Not to pop up user-level notification because they cannot do anything
        // about it.
        return;
      default:
        Cu.reportError(new Error("Unknown message ('" + status + "') dealing with EME key request: " + data));
        return;
    }

    this.showNotificationBar(browser, notificationId, keySystem, params, buttonCallback);
  },
  showNotificationBar(browser, notificationId, keySystem, labelParams, callback) {
    let box = gBrowser.getNotificationBox(browser);
    if (box.getNotificationWithValue(notificationId)) {
      return;
    }

    let msgPrefix = "emeNotifications." + notificationId + ".";
    let msgId = msgPrefix + "message";

    let message = labelParams.length ?
                  gNavigatorBundle.getFormattedString(msgId, labelParams) :
                  gNavigatorBundle.getString(msgId);

    let buttons = [];
    if (callback) {
      let btnLabelId = msgPrefix + "button.label";
      let btnAccessKeyId = msgPrefix + "button.accesskey";
      buttons.push({
        label: gNavigatorBundle.getString(btnLabelId),
        accessKey: gNavigatorBundle.getString(btnAccessKeyId),
        callback
      });
    }

    let iconURL = "chrome://browser/skin/drm-icon.svg#chains-black";

    // Do a little dance to get rich content into the notification:
    let fragment = document.createDocumentFragment();
    let descriptionContainer = document.createElement("description");
    descriptionContainer.innerHTML = message;
    while (descriptionContainer.childNodes.length) {
      fragment.appendChild(descriptionContainer.childNodes[0]);
    }

    box.appendNotification(fragment, notificationId, iconURL, box.PRIORITY_WARNING_MEDIUM,
                           buttons);
  },
  showPopupNotificationForSuccess(browser, keySystem) {
    // We're playing EME content! Remove any "we can't play because..." messages.
    var box = gBrowser.getNotificationBox(browser);
    ["drmContentDisabled",
     "drmContentCDMInstalling"
     ].forEach(function(value) {
        var notification = box.getNotificationWithValue(value);
        if (notification)
          box.removeNotification(notification);
      });

    // Don't bother creating it if it's already there:
    if (PopupNotifications.getNotification("drmContentPlaying", browser)) {
      return;
    }

    let msgPrefix = "emeNotifications.drmContentPlaying.";
    let msgId = msgPrefix + "message2";
    let btnLabelId = msgPrefix + "button.label";
    let btnAccessKeyId = msgPrefix + "button.accesskey";

    let message = gNavigatorBundle.getFormattedString(msgId, [this._brandShortName]);
    let anchorId = "eme-notification-icon";
    let firstPlayPref = "browser.eme.ui.firstContentShown";
    if (!Services.prefs.getPrefType(firstPlayPref) ||
        !Services.prefs.getBoolPref(firstPlayPref)) {
      document.getElementById(anchorId).setAttribute("firstplay", "true");
      Services.prefs.setBoolPref(firstPlayPref, true);
    } else {
      document.getElementById(anchorId).removeAttribute("firstplay");
    }

    let mainAction = {
      label: gNavigatorBundle.getString(btnLabelId),
      accessKey: gNavigatorBundle.getString(btnAccessKeyId),
      callback() { openPreferences("panePrivacy"); },
      dismiss: true
    };
    let options = {
      dismissed: true,
      eventCallback: aTopic => aTopic == "swapping",
      learnMoreURL: Services.urlFormatter.formatURLPref("app.support.baseURL") + "drm-content",
    };
    PopupNotifications.show(browser, "drmContentPlaying", message, anchorId, mainAction, null, options);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener])
};

XPCOMUtils.defineLazyGetter(gEMEHandler, "_brandShortName", function() {
  return document.getElementById("bundle_brand").getString("brandShortName");
});

const TELEMETRY_DDSTAT_SHOWN = 0;
const TELEMETRY_DDSTAT_SHOWN_FIRST = 1;
const TELEMETRY_DDSTAT_CLICKED = 2;
const TELEMETRY_DDSTAT_CLICKED_FIRST = 3;
const TELEMETRY_DDSTAT_SOLVED = 4;

let gDecoderDoctorHandler = {
  getLabelForNotificationBox(type) {
    if (type == "platform-decoder-not-found") {
      if (AppConstants.platform == "win") {
        return gNavigatorBundle.getString("decoder.noHWAcceleration.message");
      }
      if (AppConstants.platform == "linux") {
        return gNavigatorBundle.getString("decoder.noCodecsLinux.message");
      }
    }
    if (type == "cannot-initialize-pulseaudio") {
      return gNavigatorBundle.getString("decoder.noPulseAudio.message");
    }
    if (type == "unsupported-libavcodec" &&
        AppConstants.platform == "linux") {
      return gNavigatorBundle.getString("decoder.unsupportedLibavcodec.message");
    }
    if (type == "decode-error") {
      return gNavigatorBundle.getString("decoder.decodeError.message");
    }
    if (type == "decode-warning") {
      return gNavigatorBundle.getString("decoder.decodeWarning.message");
    }
    return "";
  },

  getSumoForLearnHowButton(type) {
    if (type == "platform-decoder-not-found" &&
        AppConstants.platform == "win") {
      return "fix-video-audio-problems-firefox-windows";
    }
    if (type == "cannot-initialize-pulseaudio") {
      return "fix-common-audio-and-video-issues";
    }
    return "";
  },

  getEndpointForReportIssueButton(type) {
    if (type == "decode-error" || type == "decode-warning") {
      return Services.prefs.getStringPref("media.decoder-doctor.new-issue-endpoint", "");
    }
    return "";
  },

  receiveMessage({target: browser, data: data}) {
    let box = gBrowser.getNotificationBox(browser);
    let notificationId = "decoder-doctor-notification";
    if (box.getNotificationWithValue(notificationId)) {
      return;
    }

    let parsedData;
    try {
      parsedData = JSON.parse(data);
    } catch (ex) {
      Cu.reportError("Malformed Decoder Doctor message with data: " + data);
      return;
    }
    // parsedData (the result of parsing the incoming 'data' json string)
    // contains analysis information from Decoder Doctor:
    // - 'type' is the type of issue, it determines which text to show in the
    //   infobar.
    // - 'isSolved' is true when the notification actually indicates the
    //   resolution of that issue, to be reported as telemetry.
    // - 'decoderDoctorReportId' is the Decoder Doctor issue identifier, to be
    //   used here as key for the telemetry (counting infobar displays,
    //   "Learn how" buttons clicks, and resolutions) and for the prefs used
    //   to store at-issue formats.
    // - 'formats' contains a comma-separated list of formats (or key systems)
    //   that suffer the issue. These are kept in a pref, which the backend
    //   uses to later find when an issue is resolved.
    // - 'decodeIssue' is a description of the decode error/warning.
    // - 'resourceURL' is the resource with the issue.
    let {type, isSolved, decoderDoctorReportId,
         formats, decodeIssue, docURL, resourceURL} = parsedData;
    type = type.toLowerCase();
    // Error out early on invalid ReportId
    if (!(/^\w+$/mi).test(decoderDoctorReportId)) {
      return
    }
    let title = gDecoderDoctorHandler.getLabelForNotificationBox(type);
    if (!title) {
      return;
    }

    // We keep the list of formats in prefs for the sake of the decoder itself,
    // which reads it to determine when issues get solved for these formats.
    // (Writing prefs from e10s content is not allowed.)
    let formatsPref = formats &&
                      "media.decoder-doctor." + decoderDoctorReportId + ".formats";
    let buttonClickedPref = "media.decoder-doctor." + decoderDoctorReportId + ".button-clicked";
    let histogram =
      Services.telemetry.getKeyedHistogramById("DECODER_DOCTOR_INFOBAR_STATS");

    let formatsInPref = formats &&
                        Services.prefs.getCharPref(formatsPref, "");

    if (!isSolved) {
      if (formats) {
        if (!formatsInPref) {
          Services.prefs.setCharPref(formatsPref, formats);
          histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_SHOWN_FIRST);
        } else {
          // Split existing formats into an array of strings.
          let existing = formatsInPref.split(",").map(x => x.trim());
          // Keep given formats that were not already recorded.
          let newbies = formats.split(",").map(x => x.trim())
                        .filter(x => !existing.includes(x));
          // And rewrite pref with the added new formats (if any).
          if (newbies.length) {
            Services.prefs.setCharPref(formatsPref,
                                      existing.concat(newbies).join(", "));
          }
        }
      } else if (!decodeIssue) {
        Cu.reportError("Malformed Decoder Doctor unsolved message with no formats nor decode issue");
        return;
      }
      histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_SHOWN);

      let buttons = [];
      let sumo = gDecoderDoctorHandler.getSumoForLearnHowButton(type);
      if (sumo) {
        buttons.push({
          label: gNavigatorBundle.getString("decoder.noCodecs.button"),
          accessKey: gNavigatorBundle.getString("decoder.noCodecs.accesskey"),
          callback() {
            let clickedInPref =
              Services.prefs.getBoolPref(buttonClickedPref, false);
            if (!clickedInPref) {
              Services.prefs.setBoolPref(buttonClickedPref, true);
              histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_CLICKED_FIRST);
            }
            histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_CLICKED);

            let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
            openUILinkIn(baseURL + sumo, "tab");
          }
        });
      }
      let endpoint = gDecoderDoctorHandler.getEndpointForReportIssueButton(type);
      if (endpoint) {
        buttons.push({
          label: gNavigatorBundle.getString("decoder.decodeError.button"),
          accessKey: gNavigatorBundle.getString("decoder.decodeError.accesskey"),
          callback() {
            let clickedInPref =
              Services.prefs.getBoolPref(buttonClickedPref, false);
            if (!clickedInPref) {
              Services.prefs.setBoolPref(buttonClickedPref, true);
              histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_CLICKED_FIRST);
            }
            histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_CLICKED);

            let params = new URLSearchParams;
            params.append("url", docURL);
            params.append("problem_type", "video_bug");
            params.append("src", "media-decode-error");
            params.append("details",
                          "Technical Information:\n" + decodeIssue +
                          (resourceURL ? ("\nResource: " + resourceURL) : ""));
            openUILinkIn(endpoint + "?" + params.toString(), "tab");
          }
        });
      }

      box.appendNotification(
          title,
          notificationId,
          "", // This uses the info icon as specified below.
          box.PRIORITY_INFO_LOW,
          buttons
      );
    } else if (formatsInPref) {
      // Issue is solved, and prefs haven't been cleared yet, meaning it's the
      // first time we get this resolution -> Clear prefs and report telemetry.
      Services.prefs.clearUserPref(formatsPref);
      Services.prefs.clearUserPref(buttonClickedPref);
      histogram.add(decoderDoctorReportId, TELEMETRY_DDSTAT_SOLVED);
    }
  },
}

window.getGroupMessageManager("browsers").addMessageListener("DecoderDoctor:Notification", gDecoderDoctorHandler);
window.getGroupMessageManager("browsers").addMessageListener("EMEVideo:ContentMediaKeysRequest", gEMEHandler);
window.addEventListener("unload", function() {
  window.getGroupMessageManager("browsers").removeMessageListener("EMEVideo:ContentMediaKeysRequest", gEMEHandler);
  window.getGroupMessageManager("browsers").removeMessageListener("DecoderDoctor:Notification", gDecoderDoctorHandler);
});
