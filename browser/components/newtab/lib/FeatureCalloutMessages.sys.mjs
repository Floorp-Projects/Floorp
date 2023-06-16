/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Eventually, make this a messaging system
// provider instead of adding these message
// into OnboardingMessageProvider.jsm
const FIREFOX_VIEW_PREF = "browser.firefox-view.feature-tour";
const PDFJS_PREF = "browser.pdfjs.feature-tour";
// Empty screens are included as placeholders to ensure step
// indicator shows the correct number of total steps in the tour
const EMPTY_SCREEN = { content: {} };
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;

// Generate a JEXL targeting string based on the current screen
// id found in a given Feature Callout tour progress preference
// and the `complete` property being true
const matchCurrentScreenTargeting = (prefName, screenId) => {
  const prefVal = `'${prefName}' | preferenceValue`;
  //regExpMatch() is a JEXL filter expression. Here we check if 'screen' and 'complete' exist in the pref's value (which is stringified JSON), and return their values. Returns null otherwise
  const screenRegEx = '(?<=screen":)"(.*)(?=",)';
  const completeRegEx = '(?<=complete":)(.*)(?=})';

  const screenMatch = `${prefVal}  | regExpMatch('${screenRegEx}')`;
  const completeMatch = `${prefVal}  | regExpMatch('${completeRegEx}')`;
  //We are checking the return of regExpMatch() here. If it's truthy, we grab the matched string and compare it to the desired value
  const screenVal = `(${screenMatch}) ? (${screenMatch}[1] == '${screenId}') : false`;
  const completeVal = `(${completeMatch}) ? (${completeMatch}[1] != "true") : false`;

  return `(${screenVal}) && (${completeVal})`;
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
        }[messageImpressions.${message.id} | length - 1] < ${
          Date.now() - ONE_DAY_IN_MS
        })`
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
      id: "FIREFOX_VIEW_SPOTLIGHT",
      template: "spotlight",
      content: {
        id: "FIREFOX_VIEW_PROMO",
        template: "multistage",
        modal: "tab",
        screens: [
          {
            id: "DEFAULT_MODAL_UI",
            content: {
              title: {
                fontSize: "32px",
                fontWeight: 400,
                string_id: "firefoxview-spotlight-promo-title",
              },
              subtitle: {
                fontSize: "15px",
                fontWeight: 400,
                marginBlock: "10px",
                marginInline: "40px",
                string_id: "firefoxview-spotlight-promo-subtitle",
              },
              logo: { height: "48px" },
              primary_button: {
                label: {
                  string_id: "firefoxview-spotlight-promo-primarybutton",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: FIREFOX_VIEW_PREF,
                      value: JSON.stringify({
                        screen: "FEATURE_CALLOUT_1",
                        complete: false,
                      }),
                    },
                  },
                  navigate: true,
                },
              },
              secondary_button: {
                label: {
                  string_id: "firefoxview-spotlight-promo-secondarybutton",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: FIREFOX_VIEW_PREF,
                      value: JSON.stringify({
                        screen: "",
                        complete: true,
                      }),
                    },
                  },
                  navigate: true,
                },
              },
            },
          },
        ],
      },
      priority: 3,
      trigger: {
        id: "featureCalloutCheck",
      },
      frequency: {
        // Add the highest possible cap to ensure impressions are recorded while allowing the Spotlight to sync across windows/tabs with Firefox View open
        lifetime: 100,
      },
      targeting: `!inMr2022Holdback && source == "about:firefoxview" &&
       !'browser.newtabpage.activity-stream.asrouter.providers.cfr'|preferenceIsUserSet &&
       'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features'|preferenceValue &&
       ${matchCurrentScreenTargeting(
         FIREFOX_VIEW_PREF,
         "FIREFOX_VIEW_SPOTLIGHT"
       )}`,
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
      priority: 3,
      targeting: `!inMr2022Holdback && source == "about:firefoxview" && ${matchCurrentScreenTargeting(
        FIREFOX_VIEW_PREF,
        "FEATURE_CALLOUT_1"
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
      priority: 3,
      targeting: `!inMr2022Holdback && source == "about:firefoxview" && ${matchCurrentScreenTargeting(
        FIREFOX_VIEW_PREF,
        "FEATURE_CALLOUT_2"
      )}`,
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
      targeting: `!inMr2022Holdback && source == "about:firefoxview" && "browser.firefox-view.view-count" | preferenceValue > 2
    && (("identity.fxaccounts.enabled" | preferenceValue == false) || !(("services.sync.engine.tabs" | preferenceValue == true) && ("services.sync.username" | preferenceValue))) && (!messageImpressions.FIREFOX_VIEW_SPOTLIGHT[messageImpressions.FIREFOX_VIEW_SPOTLIGHT | length - 1] || messageImpressions.FIREFOX_VIEW_SPOTLIGHT[messageImpressions.FIREFOX_VIEW_SPOTLIGHT | length - 1] < currentDate|date - ${ONE_DAY_IN_MS})`,
      frequency: {
        lifetime: 1,
      },
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_1_A",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FEATURE_CALLOUT_1_A",
            parent_selector: "hbox#browser",
            content: {
              position: "callout",
              callout_position_override: {
                top: "45px",
                right: "55px",
              },
              arrow_position: "top-end",
              title: {
                string_id: "callout-pdfjs-edit-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-edit-body-a",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-edit-button",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "FEATURE_CALLOUT_2_A",
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
                      name: PDFJS_PREF,
                      value: JSON.stringify({
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
      targeting: `source == "chrome" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_1_A"
      )}`,
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_2_A",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        startScreen: 1,
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          EMPTY_SCREEN,
          {
            id: "FEATURE_CALLOUT_2_A",
            parent_selector: "hbox#browser",
            content: {
              position: "callout",
              callout_position_override: {
                top: "45px",
                right: "25px",
              },
              arrow_position: "top-end",
              title: {
                string_id: "callout-pdfjs-draw-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-draw-body-a",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-draw-button",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
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
                      name: PDFJS_PREF,
                      value: JSON.stringify({
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
      targeting: `source == "chrome" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_2_A"
      )}`,
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_1_B",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FEATURE_CALLOUT_1_B",
            parent_selector: "hbox#browser",
            content: {
              position: "callout",
              callout_position_override: {
                top: "45px",
                right: "55px",
              },
              arrow_position: "top-end",
              title: {
                string_id: "callout-pdfjs-edit-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-edit-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-edit-button",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
                        screen: "FEATURE_CALLOUT_2_B",
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
                      name: PDFJS_PREF,
                      value: JSON.stringify({
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
      targeting: `source == "chrome" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_1_B"
      )}`,
      trigger: { id: "featureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_2_B",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        startScreen: 1,
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          EMPTY_SCREEN,
          {
            id: "FEATURE_CALLOUT_2_B",
            parent_selector: "hbox#browser",
            content: {
              position: "callout",
              callout_position_override: {
                top: "45px",
                right: "25px",
              },
              arrow_position: "top-end",
              title: {
                string_id: "callout-pdfjs-draw-title",
              },
              subtitle: {
                string_id: "callout-pdfjs-draw-body-b",
              },
              primary_button: {
                label: {
                  string_id: "callout-pdfjs-draw-button",
                },
                action: {
                  type: "SET_PREF",
                  data: {
                    pref: {
                      name: PDFJS_PREF,
                      value: JSON.stringify({
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
                      name: PDFJS_PREF,
                      value: JSON.stringify({
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
      targeting: `source == "chrome" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_2_B"
      )}`,
      trigger: { id: "featureCalloutCheck" },
    },
  ];
  messages = add24HourImpressionJEXLTargeting(
    ["FIREFOX_VIEW_TAB_PICKUP_REMINDER"],
    "FIREFOX_VIEW",
    messages
  );
  return messages;
};

export const FeatureCalloutMessages = {
  getMessages() {
    return MESSAGES();
  },
};
