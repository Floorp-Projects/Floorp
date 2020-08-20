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

export const DEFAULT_WELCOME_CONTENT = {
  template: "multistage",
  screens: [
    {
      id: "AW_GET_STARTED",
      order: 0,
      content: {
        zap: true,
        title: {
          string_id: "onboarding-multistage-welcome-header",
        },
        subtitle: { string_id: "onboarding-multistage-welcome-subtitle" },
        primary_button: {
          label: {
            string_id: "onboarding-multistage-welcome-primary-button-label",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          text: {
            string_id: "onboarding-multistage-welcome-secondary-button-text",
          },
          label: {
            string_id: "onboarding-multistage-welcome-secondary-button-label",
          },
          position: "top",
          action: {
            type: "SHOW_FIREFOX_ACCOUNTS",
            addFlowParams: true,
            data: {
              entrypoint: "activity-stream-firstrun",
            },
          },
        },
      },
    },
    {
      id: "AW_IMPORT_SETTINGS",
      order: 1,
      content: {
        zap: true,
        disclaimer: { string_id: "onboarding-import-sites-disclaimer" },
        title: { string_id: "onboarding-multistage-import-header" },
        subtitle: { string_id: "onboarding-multistage-import-subtitle" },
        tiles: {
          type: "topsites",
          info: true,
        },
        primary_button: {
          label: {
            string_id: "onboarding-multistage-import-primary-button-label",
          },
          action: {
            type: "SHOW_MIGRATION_WIZARD",
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "onboarding-multistage-import-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_CHOOSE_THEME",
      order: 2,
      content: {
        zap: true,
        title: { string_id: "onboarding-multistage-theme-header" },
        subtitle: { string_id: "onboarding-multistage-theme-subtitle" },
        tiles: {
          type: "theme",
          action: {
            theme: "<event>",
          },
          data: [
            {
              theme: "automatic",
              label: {
                string_id: "onboarding-multistage-theme-label-automatic",
              },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-automatic",
              },
            },
            {
              theme: "light",
              label: { string_id: "onboarding-multistage-theme-label-light" },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-light",
              },
            },
            {
              theme: "dark",
              label: { string_id: "onboarding-multistage-theme-label-dark" },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-dark",
              },
            },
            {
              theme: "alpenglow",
              label: {
                string_id: "onboarding-multistage-theme-label-alpenglow",
              },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-alpenglow",
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "onboarding-multistage-theme-primary-button-label",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "onboarding-multistage-theme-secondary-button-label",
          },
          action: {
            theme: "automatic",
            navigate: true,
          },
        },
      },
    },
  ],
};
