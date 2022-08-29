/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Eventually, make this a messaging system
// provider instead of adding these message
// into OnboardingMessageProvider.jsm
const FIREFOX_VIEW_PREF = "browser.firefox-view.feature-tour";
// Empty screens are included as placeholders to ensure step
// indicator shows the correct number of total steps in the tour
const EMPTY_SCREEN = { content: {} };
// Generate a JEXL targeting string based on the current screen
// id found in a given Feature Callout tour progress preference
const matchCurrentScreenTargeting = (prefName, screenId) => {
  return `'${prefName}' | preferenceValue | regExpMatch('(?<=screen\"\:)"(.*)(?=",)')[1] == '${screenId}'`;
};

const MESSAGES = () => [
  // about:firefoxview messages
  {
    id: "FIREFOX_VIEW_FEATURE_TOUR_1",
    template: "feature_callout",
    content: {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        {
          id: "FEATURE_CALLOUT_1",
          parent_selector: "#tabpickup-steps",
          content: {
            position: "callout",
            arrow_position: "top",
            title: {
              string_id: "callout-firefox-view-tab-pickup-title",
            },
            subtitle: {
              string_id: "callout-firefox-view-tab-pickup-subtitle",
            },
            logo: {
              imageURL: "chrome://browser/content/callout-tab-pickup.svg",
              darkModeImageURL:
                "chrome://browser/content/callout-tab-pickup-dark.svg",
              height: "128px",
            },
            primary_button: {
              label: {
                string_id: "callout-primary-advance-button-label",
              },
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "FEATURE_CALLOUT_2",
                      complete: false,
                    }),
                  },
                },
              },
            },
            dismiss_button: {
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "",
                      complete: true,
                    }),
                  },
                },
              },
            },
          },
        },
        EMPTY_SCREEN,
        EMPTY_SCREEN,
      ],
    },
    priority: 1,
    targeting: `source == "firefoxview" && colorwaysActive && ${matchCurrentScreenTargeting(
      FIREFOX_VIEW_PREF,
      "FEATURE_CALLOUT_1"
    )}`,
    trigger: { id: "featureCalloutCheck" },
  },
  {
    id: "FIREFOX_VIEW_FEATURE_TOUR_1_NO_CWS",
    template: "feature_callout",
    content: {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        {
          id: "FEATURE_CALLOUT_1",
          parent_selector: "#tabpickup-steps",
          content: {
            position: "callout",
            arrow_position: "top",
            title: {
              string_id: "callout-firefox-view-tab-pickup-title",
            },
            subtitle: {
              string_id: "callout-firefox-view-tab-pickup-subtitle",
            },
            logo: {
              imageURL: "chrome://browser/content/callout-tab-pickup.svg",
              darkModeImageURL:
                "chrome://browser/content/callout-tab-pickup-dark.svg",
              height: "128px",
            },
            primary_button: {
              label: {
                string_id: "callout-primary-advance-button-label",
              },
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "FEATURE_CALLOUT_2",
                      complete: false,
                    }),
                  },
                },
              },
            },
            dismiss_button: {
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "",
                      complete: true,
                    }),
                  },
                },
              },
            },
          },
        },
        EMPTY_SCREEN,
      ],
    },
    priority: 1,
    targeting: `source == "firefoxview" && !colorwaysActive && ${matchCurrentScreenTargeting(
      FIREFOX_VIEW_PREF,
      "FEATURE_CALLOUT_1"
    )}`,
    trigger: { id: "featureCalloutCheck" },
  },
  {
    id: "FIREFOX_VIEW_FEATURE_TOUR_2",
    template: "feature_callout",
    content: {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      startScreen: 1,
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        EMPTY_SCREEN,
        {
          id: "FEATURE_CALLOUT_2",
          parent_selector: "#recently-closed-tabs-container",
          content: {
            position: "callout",
            arrow_position: "bottom",
            title: {
              string_id: "callout-firefox-view-recently-closed-title",
            },
            subtitle: {
              string_id: "callout-firefox-view-recently-closed-subtitle",
            },
            primary_button: {
              label: {
                string_id: "callout-primary-advance-button-label",
              },
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "FEATURE_CALLOUT_3",
                      complete: false,
                    }),
                  },
                },
              },
            },
            dismiss_button: {
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "",
                      complete: true,
                    }),
                  },
                },
              },
            },
          },
        },
        EMPTY_SCREEN,
      ],
    },
    priority: 1,
    targeting: `source == "firefoxview" && colorwaysActive && ${matchCurrentScreenTargeting(
      FIREFOX_VIEW_PREF,
      "FEATURE_CALLOUT_2"
    )}`,
    trigger: { id: "featureCalloutCheck" },
  },
  {
    id: "FIREFOX_VIEW_FEATURE_TOUR_2_NO_CWS",
    template: "feature_callout",
    content: {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      startScreen: 1,
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        EMPTY_SCREEN,
        {
          id: "FEATURE_CALLOUT_2",
          parent_selector: "#recently-closed-tabs-container",
          content: {
            position: "callout",
            arrow_position: "bottom",
            title: {
              string_id: "callout-firefox-view-recently-closed-title",
            },
            subtitle: {
              string_id: "callout-firefox-view-recently-closed-subtitle",
            },
            primary_button: {
              label: {
                string_id: "callout-primary-complete-button-label",
              },
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "FEATURE_CALLOUT_3",
                      complete: false,
                    }),
                  },
                },
              },
            },
            dismiss_button: {
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "",
                      complete: true,
                    }),
                  },
                },
              },
            },
          },
        },
      ],
    },
    priority: 1,
    targeting: `source == "firefoxview" && !colorwaysActive && ${matchCurrentScreenTargeting(
      FIREFOX_VIEW_PREF,
      "FEATURE_CALLOUT_2"
    )}`,
    trigger: { id: "featureCalloutCheck" },
  },
  {
    id: "FIREFOX_VIEW_FEATURE_TOUR_3",
    template: "feature_callout",
    content: {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      startScreen: 2,
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        EMPTY_SCREEN,
        EMPTY_SCREEN,
        {
          id: "FEATURE_CALLOUT_3",
          parent_selector: "#colorways.content-container",
          content: {
            position: "callout",
            arrow_position: "end",
            title: {
              string_id: "callout-firefox-view-colorways-title",
            },
            subtitle: {
              string_id: "callout-firefox-view-colorways-subtitle",
            },
            logo: {
              imageURL: "chrome://browser/content/callout-colorways.svg",
              darkModeImageURL:
                "chrome://browser/content/callout-colorways-dark.svg",
              height: "128px",
            },
            primary_button: {
              label: {
                string_id: "callout-primary-complete-button-label",
              },
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "",
                      complete: true,
                    }),
                  },
                },
              },
            },
            dismiss_button: {
              action: {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: FIREFOX_VIEW_PREF,
                    value: JSON.stringify({
                      message: "FIREFOX_VIEW_FEATURE_TOUR",
                      screen: "",
                      complete: true,
                    }),
                  },
                },
              },
            },
          },
        },
      ],
    },
    priority: 1,
    targeting: `source == "firefoxview" && colorwaysActive && ${matchCurrentScreenTargeting(
      FIREFOX_VIEW_PREF,
      "FEATURE_CALLOUT_3"
    )}`,
    trigger: { id: "featureCalloutCheck" },
  },
];

const FeatureCalloutMessages = {
  getMessages() {
    return MESSAGES();
  },
};

const EXPORTED_SYMBOLS = ["FeatureCalloutMessages"];
