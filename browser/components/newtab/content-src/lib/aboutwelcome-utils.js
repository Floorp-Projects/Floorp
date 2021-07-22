/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const AboutWelcomeUtils = {
  handleUserAction(action) {
    window.AWSendToParent("SPECIAL_ACTION", action);
  },
  sendImpressionTelemetry(messageId, context) {
    window.AWSendEventTelemetry({
      event: "IMPRESSION",
      event_context: context,
      message_id: messageId,
    });
  },
  sendActionTelemetry(messageId, elementId) {
    const ping = {
      event: "CLICK_BUTTON",
      event_context: {
        source: elementId,
        page: "about:welcome",
      },
      message_id: messageId,
    };
    window.AWSendEventTelemetry(ping);
  },
  async fetchFlowParams(metricsFlowUri) {
    let flowParams;
    try {
      const response = await fetch(metricsFlowUri, {
        credentials: "omit",
      });
      if (response.status === 200) {
        const { deviceId, flowId, flowBeginTime } = await response.json();
        flowParams = { deviceId, flowId, flowBeginTime };
      } else {
        console.error("Non-200 response", response); // eslint-disable-line no-console
      }
    } catch (e) {
      flowParams = null;
    }
    return flowParams;
  },
  sendEvent(type, detail) {
    document.dispatchEvent(
      new CustomEvent(`AWPage:${type}`, {
        bubbles: true,
        detail,
      })
    );
  },
  hasDarkMode() {
    return document.body.hasAttribute("lwt-newtab-brighttext");
  },
};

export const DEFAULT_RTAMO_CONTENT = {
  template: "return_to_amo",
  content: {
    header: { string_id: "onboarding-welcome-header" },
    subtitle: { string_id: "return-to-amo-subtitle" },
    text: {
      string_id: "return-to-amo-addon-title",
    },
    primary_button: {
      label: { string_id: "return-to-amo-add-extension-label" },
      action: {
        type: "INSTALL_ADDON_FROM_URL",
        data: { url: null, telemetrySource: "rtamo" },
      },
    },
    startButton: {
      label: {
        string_id: "onboarding-not-now-button-label",
      },
      message_id: "RTAMO_START_BROWSING_BUTTON",
      action: {
        type: "OPEN_AWESOME_BAR",
      },
    },
  },
};
