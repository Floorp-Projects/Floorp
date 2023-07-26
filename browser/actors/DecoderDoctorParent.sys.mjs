/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "gNavigatorBundle", function () {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DEBUG_LOG",
  "media.decoder-doctor.testing",
  false
);

function LOG_DD(message) {
  if (lazy.DEBUG_LOG) {
    dump("[DecoderDoctorParent] " + message + "\n");
  }
}

export class DecoderDoctorParent extends JSWindowActorParent {
  getLabelForNotificationBox({ type, decoderDoctorReportId }) {
    if (type == "platform-decoder-not-found") {
      if (decoderDoctorReportId == "MediaWMFNeeded") {
        return lazy.gNavigatorBundle.GetStringFromName(
          "decoder.noHWAcceleration.message"
        );
      }
      // Although this name seems generic, this is actually for not being able
      // to find libavcodec on Linux.
      if (decoderDoctorReportId == "MediaPlatformDecoderNotFound") {
        return lazy.gNavigatorBundle.GetStringFromName(
          "decoder.noCodecsLinux.message"
        );
      }
    }
    if (type == "cannot-initialize-pulseaudio") {
      return lazy.gNavigatorBundle.GetStringFromName(
        "decoder.noPulseAudio.message"
      );
    }
    if (type == "unsupported-libavcodec" && AppConstants.platform == "linux") {
      return lazy.gNavigatorBundle.GetStringFromName(
        "decoder.unsupportedLibavcodec.message"
      );
    }
    if (type == "decode-error") {
      return lazy.gNavigatorBundle.GetStringFromName(
        "decoder.decodeError.message"
      );
    }
    if (type == "decode-warning") {
      return lazy.gNavigatorBundle.GetStringFromName(
        "decoder.decodeWarning.message"
      );
    }
    return "";
  }

  getSumoForLearnHowButton({ type, decoderDoctorReportId }) {
    if (
      type == "platform-decoder-not-found" &&
      decoderDoctorReportId == "MediaWMFNeeded"
    ) {
      return "fix-video-audio-problems-firefox-windows";
    }
    if (type == "cannot-initialize-pulseaudio") {
      return "fix-common-audio-and-video-issues";
    }
    return "";
  }

  getEndpointForReportIssueButton(type) {
    if (type == "decode-error" || type == "decode-warning") {
      return Services.prefs.getStringPref(
        "media.decoder-doctor.new-issue-endpoint",
        ""
      );
    }
    return "";
  }

  receiveMessage(aMessage) {
    // The top level browsing context's embedding element should be a xul browser element.
    let browser = this.browsingContext.top.embedderElement;
    // The xul browser is owned by a window.
    let window = browser?.ownerGlobal;

    if (!browser || !window) {
      // We don't have a browser or window so bail!
      return;
    }

    let box = browser.getTabBrowser().getNotificationBox(browser);
    let notificationId = "decoder-doctor-notification";
    if (box.getNotificationWithValue(notificationId)) {
      // We already have a notification showing, bail.
      return;
    }

    let parsedData;
    try {
      parsedData = JSON.parse(aMessage.data);
    } catch (ex) {
      console.error(
        "Malformed Decoder Doctor message with data: ",
        aMessage.data
      );
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
    let {
      type,
      isSolved,
      decoderDoctorReportId,
      formats,
      decodeIssue,
      docURL,
      resourceURL,
    } = parsedData;
    type = type.toLowerCase();
    // Error out early on invalid ReportId
    if (!/^\w+$/im.test(decoderDoctorReportId)) {
      return;
    }
    LOG_DD(
      `type=${type}, isSolved=${isSolved}, ` +
        `decoderDoctorReportId=${decoderDoctorReportId}, formats=${formats}, ` +
        `decodeIssue=${decodeIssue}, docURL=${docURL}, ` +
        `resourceURL=${resourceURL}`
    );
    let title = this.getLabelForNotificationBox({
      type,
      decoderDoctorReportId,
    });
    if (!title) {
      return;
    }

    // We keep the list of formats in prefs for the sake of the decoder itself,
    // which reads it to determine when issues get solved for these formats.
    // (Writing prefs from e10s content is not allowed.)
    let formatsPref =
      formats && "media.decoder-doctor." + decoderDoctorReportId + ".formats";
    let buttonClickedPref =
      "media.decoder-doctor." + decoderDoctorReportId + ".button-clicked";
    let formatsInPref = formats && Services.prefs.getCharPref(formatsPref, "");

    if (!isSolved) {
      if (formats) {
        if (!formatsInPref) {
          Services.prefs.setCharPref(formatsPref, formats);
        } else {
          // Split existing formats into an array of strings.
          let existing = formatsInPref.split(",").map(x => x.trim());
          // Keep given formats that were not already recorded.
          let newbies = formats
            .split(",")
            .map(x => x.trim())
            .filter(x => !existing.includes(x));
          // And rewrite pref with the added new formats (if any).
          if (newbies.length) {
            Services.prefs.setCharPref(
              formatsPref,
              existing.concat(newbies).join(", ")
            );
          }
        }
      } else if (!decodeIssue) {
        console.error(
          "Malformed Decoder Doctor unsolved message with no formats nor decode issue"
        );
        return;
      }

      let buttons = [];
      let sumo = this.getSumoForLearnHowButton({ type, decoderDoctorReportId });
      if (sumo) {
        LOG_DD(`sumo=${sumo}`);
        buttons.push({
          label: lazy.gNavigatorBundle.GetStringFromName(
            "decoder.noCodecs.button"
          ),
          supportPage: sumo,
          callback() {
            let clickedInPref = Services.prefs.getBoolPref(
              buttonClickedPref,
              false
            );
            if (!clickedInPref) {
              Services.prefs.setBoolPref(buttonClickedPref, true);
            }
          },
        });
      }
      let endpoint = this.getEndpointForReportIssueButton(type);
      if (endpoint) {
        LOG_DD(`endpoint=${endpoint}`);
        buttons.push({
          label: lazy.gNavigatorBundle.GetStringFromName(
            "decoder.decodeError.button"
          ),
          accessKey: lazy.gNavigatorBundle.GetStringFromName(
            "decoder.decodeError.accesskey"
          ),
          callback() {
            let clickedInPref = Services.prefs.getBoolPref(
              buttonClickedPref,
              false
            );
            if (!clickedInPref) {
              Services.prefs.setBoolPref(buttonClickedPref, true);
            }

            let params = new URLSearchParams();
            params.append("url", docURL);
            params.append("label", "type-media");
            params.append("problem_type", "video_bug");
            params.append("src", "media-decode-error");

            let details = { "Technical Information:": decodeIssue };
            if (resourceURL) {
              details["Resource:"] = resourceURL;
            }

            params.append("details", JSON.stringify(details));
            window.openTrustedLinkIn(endpoint + "?" + params.toString(), "tab");
          },
        });
      }

      box.appendNotification(
        notificationId,
        {
          label: title,
          image: "", // This uses the info icon as specified below.
          priority: box.PRIORITY_INFO_LOW,
        },
        buttons
      );
    } else if (formatsInPref) {
      // Issue is solved, and prefs haven't been cleared yet, meaning it's the
      // first time we get this resolution -> Clear prefs and report telemetry.
      Services.prefs.clearUserPref(formatsPref);
      Services.prefs.clearUserPref(buttonClickedPref);
    }
  }
}
