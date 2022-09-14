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

const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;

// Generate a JEXL targeting string based on the current screen
// id found in a given Feature Callout tour progress preference
// and the `complete` property being true
const matchCurrentScreenTargeting = (prefName, screenId) => {
  return `'${prefName}' | preferenceValue | regExpMatch('(?<=screen\"\:)"(.*)(?=",)')[1] == '${screenId}' && '${prefName}' | preferenceValue | regExpMatch('(?<=complete\"\:)(.*)(?=})')[1] != "true"`;
};

/**
 * add24HourImpressionJEXLTargeting -
 * Creates a "hasn't been viewed in > 24 hours"
 * JEXL string and adds it to each message specified
 *
 * @param {array} messageIds - IDs of messages that the targeting string will be added to
 * @param {string} prefix - The prefix of messageIDs that will used to create the JEXL string
 * @param {array} messages - The array of messages that will be edited
 * @returns {array} - The array of messages with the appropriate targeting strings edited
 */
function add24HourImpressionJEXLTargeting(
  messageIds,
  prefix,
  uneditedMessages
) {
  let noImpressionsIn24HoursString = uneditedMessages
    .filter(message => message.id.startsWith(prefix))
    .map(
      message =>
        // If the last impression is null or if epoch time
        // of the impression is < current time - 24hours worth of MS
        `(messageImpressions.${message.id}[messageImpressions.${
          message.id
        } | length - 1] == null || messageImpressions.${
          message.id
        }[messageImpressions.${message.id} | length - 1] < ${Date.now() -
          ONE_DAY_IN_MS})`
    )
    .join(" && ");

  // We're appending the string here instead of using
  // template strings to avoid a recursion error from
  // using the 'messages' variable within itself
  return uneditedMessages.map(message => {
    if (messageIds.includes(message.id)) {
      message.targeting += `&& ${noImpressionsIn24HoursString}`;
    }

    return message;
  });
}

// Exporting the about:firefoxview messages as a method here
// acts as a safety guard against mutations of the original objects
const MESSAGES = () => {
  let messages = [
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
            parent_selector: "#tab-pickup-container",
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
      targeting: `!inMr2022Holdback && source == "firefoxview" && colorwaysActive && ${matchCurrentScreenTargeting(
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
            parent_selector: "#tab-pickup-container",
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
      targeting: `!inMr2022Holdback && source == "firefoxview" && !colorwaysActive && ${matchCurrentScreenTargeting(
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
      targeting: `!inMr2022Holdback && source == "firefoxview" && colorwaysActive && ${matchCurrentScreenTargeting(
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
      targeting: `!inMr2022Holdback && source == "firefoxview" && !colorwaysActive && ${matchCurrentScreenTargeting(
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
      targeting: `!inMr2022Holdback && source == "firefoxview" && colorwaysActive && ${matchCurrentScreenTargeting(
        FIREFOX_VIEW_PREF,
        "FEATURE_CALLOUT_3"
      )}`,
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "FIREFOX_VIEW_COLORWAYS_REMINDER",
      template: "feature_callout",
      content: {
        id: "FIREFOX_VIEW_COLORWAYS_REMINDER",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FIREFOX_VIEW_COLORWAYS_REMINDER",
            parent_selector: "#colorways-collection-description",
            content: {
              position: "callout",
              arrow_position: "end",
              noCalloutOverlap: true,
              title: {
                string_id: "callout-firefox-view-colorways-reminder-title",
              },
              subtitle: {
                string_id: "callout-firefox-view-colorways-reminder-subtitle",
              },
              dismiss_button: {
                action: {
                  navigate: true,
                },
              },
            },
          },
        ],
      },
      priority: 3,
      targeting: `!inMr2022Holdback && source == "firefoxview" && "browser.firefox-view.view-count" | preferenceValue > 3 && colorwaysActive && !userEnabledActiveColorway`,
      frequency: { lifetime: 1 },
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "FIREFOX_VIEW_TAB_PICKUP_REMINDER",
      template: "feature_callout",
      content: {
        id: "FIREFOX_VIEW_TAB_PICKUP_REMINDER",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FIREFOX_VIEW_TAB_PICKUP_REMINDER",
            parent_selector: "#tab-pickup-container",
            content: {
              position: "callout",
              arrow_position: "top",
              title: {
                string_id:
                  "continuous-onboarding-firefox-view-tab-pickup-title",
              },
              subtitle: {
                string_id:
                  "continuous-onboarding-firefox-view-tab-pickup-subtitle",
              },
              logo: {
                imageURL: "chrome://browser/content/callout-tab-pickup.svg",
                darkModeImageURL:
                  "chrome://browser/content/callout-tab-pickup-dark.svg",
                height: "128px",
              },
              primary_button: {
                label: {
                  string_id: "mr1-onboarding-get-started-primary-button-label",
                },
                action: {
                  type: "CLICK_ELEMENT",
                  navigate: true,
                  data: {
                    selector:
                      "#tab-pickup-container button.primary:not(#error-state-button)",
                  },
                },
              },
              dismiss_button: {
                action: {
                  navigate: true,
                },
              },
            },
          },
        ],
      },
      priority: 2,
      targeting: `!inMr2022Holdback && source == "firefoxview" && "browser.firefox-view.view-count" | preferenceValue > 2
    && (("identity.fxaccounts.enabled" | preferenceValue == false) || !(("services.sync.engine.tabs" | preferenceValue == true) && ("services.sync.username" | preferenceValue)))`,
      frequency: {
        lifetime: 1,
      },
      trigger: { id: "featureCalloutCheck" },
    },
  ];

  messages = add24HourImpressionJEXLTargeting(
    ["FIREFOX_VIEW_COLORWAYS_REMINDER", "FIREFOX_VIEW_TAB_PICKUP_REMINDER"],
    "FIREFOX_VIEW",
    messages
  );
  return messages;
};

const FeatureCalloutMessages = {
  getMessages() {
    return MESSAGES();
  },
};

const EXPORTED_SYMBOLS = ["FeatureCalloutMessages"];
