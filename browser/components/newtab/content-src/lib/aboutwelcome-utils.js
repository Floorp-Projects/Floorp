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
};

export const DEFAULT_RTAMO_CONTENT = {
  template: "return_to_amo",
  utm_term: "rtamo",
  totalNumberOfScreens: 1,
  order: 0,
  content: {
    hero_text: { string_id: "mr1-welcome-screen-hero-text" },
    title: { string_id: "return-to-amo-subtitle" },
    subtitle: {
      string_id: "return-to-amo-addon-title",
    },
    help_text: {
      text: { string_id: "mr1-onboarding-welcome-image-caption" },
    },
    hideLogo: true,
    background_url:
      "chrome://activity-stream/content/data/content/assets/proton-bkg.avif",
    primary_button: {
      label: { string_id: "return-to-amo-add-extension-label" },
      source_id: "ADD_EXTENSION_BUTTON",
      action: {
        type: "INSTALL_ADDON_FROM_URL",
        data: { url: null, telemetrySource: "rtamo" },
      },
    },
    secondary_button: {
      label: {
        string_id: "onboarding-not-now-button-label",
      },
      source_id: "RTAMO_START_BROWSING_BUTTON",
      action: {
        type: "OPEN_AWESOME_BAR",
      },
    },
    secondary_button_top: {
      label: {
        string_id: "mr1-onboarding-sign-in-button-label",
      },
      source_id: "RTAMO_FXA_SIGNIN_BUTTON",
      action: {
        data: {
          entrypoint: "activity-stream-firstrun",
        },
        type: "SHOW_FIREFOX_ACCOUNTS",
        addFlowParams: true,
      },
    },
  },
};
