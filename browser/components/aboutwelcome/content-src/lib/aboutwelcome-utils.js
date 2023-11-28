/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// If the container has a "page" data attribute, then this is
// a Spotlight modal or Feature Callout. Otherwise, this is
// about:welcome and we should return the current page.
const page =
  document.querySelector(
    "#multi-stage-message-root.onboardingContainer[data-page]"
  )?.dataset.page || document.location.href;

export const AboutWelcomeUtils = {
  handleUserAction(action) {
    return window.AWSendToParent("SPECIAL_ACTION", action);
  },
  sendImpressionTelemetry(messageId, context) {
    window.AWSendEventTelemetry?.({
      event: "IMPRESSION",
      event_context: {
        ...context,
        page,
      },
      message_id: messageId,
    });
  },
  sendActionTelemetry(messageId, elementId, eventName = "CLICK_BUTTON") {
    const ping = {
      event: eventName,
      event_context: {
        source: elementId,
        page,
      },
      message_id: messageId,
    };
    window.AWSendEventTelemetry?.(ping);
  },
  sendDismissTelemetry(messageId, elementId) {
    // Don't send DISMISS telemetry in spotlight modals since they already send
    // their own equivalent telemetry.
    if (page !== "spotlight") {
      this.sendActionTelemetry(messageId, elementId, "DISMISS");
    }
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
        console.error("Non-200 response", response);
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
  getLoadingStrategyFor(url) {
    return url?.startsWith("http") ? "lazy" : "eager";
  },
};

export const DEFAULT_RTAMO_CONTENT = {
  template: "return_to_amo",
  utm_term: "rtamo",
  content: {
    position: "split",
    title: { string_id: "mr1-return-to-amo-subtitle" },
    has_noodles: false,
    subtitle: {
      string_id: "mr1-return-to-amo-addon-title",
    },
    backdrop:
      "var(--mr-welcome-background-color) var(--mr-welcome-background-gradient)",
    background:
      "url('chrome://activity-stream/content/data/content/assets/mr-rtamo-background-image.svg') no-repeat center",
    progress_bar: true,
    primary_button: {
      label: { string_id: "mr1-return-to-amo-add-extension-label" },
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
          where: "tab",
        },
        type: "SHOW_FIREFOX_ACCOUNTS",
        addFlowParams: true,
      },
    },
  },
};
