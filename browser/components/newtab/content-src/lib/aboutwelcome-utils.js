/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const AboutWelcomeUtils = {
  handleUserAction({ data: action }) {
    switch (action.type) {
      case "OPEN_URL":
        window.open(action.data.args);
        break;
    }
  },
};

export const DEFAULT_WELCOME_CONTENT = {
  title: {
    string_id: "onboarding-welcome-header",
  },
  subtitle: {
    string_id: "onboarding-fullpage-welcome-subheader",
  },
  cards: [
    {
      content: {
        title: {
          string_id: "onboarding-data-sync-title",
        },
        text: {
          string_id: "onboarding-data-sync-text2",
        },
        icon: "devices",
        primary_button: {
          label: {
            string_id: "onboarding-data-sync-button2",
          },
          action: {
            type: "OPEN_URL",
            addFlowParams: true,
            data: {
              args:
                "https://accounts.firefox.com/?service=sync&action=email&context=fx_desktop_v3&entrypoint=activity-stream-firstrun&style=trailhead",
              where: "tabshifted",
            },
          },
        },
      },
      id: "TRAILHEAD_CARD_2",
      order: 1,
      blockOnClick: false,
    },
    {
      content: {
        title: {
          string_id: "onboarding-firefox-monitor-title",
        },
        text: {
          string_id: "onboarding-firefox-monitor-text2",
        },
        icon: "ffmonitor",
        primary_button: {
          label: {
            string_id: "onboarding-firefox-monitor-button",
          },
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://monitor.firefox.com/",
              where: "tabshifted",
            },
          },
        },
      },
      id: "TRAILHEAD_CARD_3",
      order: 2,
      blockOnClick: false,
    },
    {
      content: {
        title: {
          string_id: "onboarding-mobile-phone-title",
        },
        text: {
          string_id: "onboarding-mobile-phone-text",
        },
        icon: "mobile",
        primary_button: {
          label: {
            string_id: "onboarding-mobile-phone-button",
          },
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/firefox/mobile/",
              where: "tabshifted",
            },
          },
        },
      },
      id: "TRAILHEAD_CARD_6",
      order: 6,
      blockOnClick: false,
    },
  ],
};
