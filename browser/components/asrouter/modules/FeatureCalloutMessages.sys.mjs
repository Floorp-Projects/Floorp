/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Eventually, make this a messaging system
// provider instead of adding these message
// into OnboardingMessageProvider.sys.mjs
const FIREFOX_VIEW_PREF = "browser.firefox-view.feature-tour";
const PDFJS_PREF = "browser.pdfjs.feature-tour";
// Empty screens are included as placeholders to ensure step
// indicator shows the correct number of total steps in the tour
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;

// Generate a JEXL targeting string based on the `complete` property being true
// in a given Feature Callout tour progress preference value (which is JSON).
const matchIncompleteTargeting = (prefName, defaultValue = false) => {
  // regExpMatch() is a JEXL filter expression. Here we check if 'complete'
  // exists in the pref's value, and returns true if the tour is incomplete.
  const prefVal = `'${prefName}' | preferenceValue`;
  // prefVal might be null if the preference doesn't exist. in this case, don't
  // try to pipe into regExpMatch.
  const completeMatch = `${prefVal} | regExpMatch('(?<=complete":)(.*)(?=})')`;
  return `((${prefVal}) ? ((${completeMatch}) ? (${completeMatch}[1] != "true") : ${String(
    defaultValue
  )}) : ${String(defaultValue)})`;
};

// Generate a JEXL targeting string based on the current screen id found in a
// given Feature Callout tour progress preference.
const matchCurrentScreenTargeting = (prefName, screenIdRegEx = ".*") => {
  // regExpMatch() is a JEXL filter expression. Here we check if 'screen' exists
  // in the pref's value, and if it matches the screenIdRegEx. Returns
  // null otherwise.
  const prefVal = `'${prefName}' | preferenceValue`;
  const screenMatch = `${prefVal} | regExpMatch('(?<=screen"\s*:)\s*"(${screenIdRegEx})(?="\s*,)')`;
  const screenValMatches = `(${screenMatch}) ? !!(${screenMatch}[1]) : false`;
  return `(${screenValMatches})`;
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
        tour_pref_name: FIREFOX_VIEW_PREF,
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
       )} && ${matchIncompleteTargeting(FIREFOX_VIEW_PREF)}`,
    },
    {
      id: "FIREFOX_VIEW_FEATURE_TOUR",
      template: "feature_callout",
      content: {
        id: "FIREFOX_VIEW_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        tour_pref_name: FIREFOX_VIEW_PREF,
        screens: [
          {
            id: "FEATURE_CALLOUT_1",
            anchors: [
              {
                selector: "#tab-pickup-container",
                arrow_position: "top",
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
              page_event_listeners: [
                {
                  params: {
                    type: "toggle",
                    selectors: "#tab-pickup-container",
                  },
                  action: { reposition: true },
                },
              ],
            },
          },
          {
            id: "FEATURE_CALLOUT_2",
            anchors: [
              {
                selector: "#recently-closed-tabs-container",
                arrow_position: "bottom",
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
              page_event_listeners: [
                {
                  params: {
                    type: "toggle",
                    selectors: "#recently-closed-tabs-container",
                  },
                  action: { reposition: true },
                },
              ],
            },
          },
        ],
      },
      priority: 3,
      targeting: `!inMr2022Holdback && source == "about:firefoxview" && ${matchCurrentScreenTargeting(
        FIREFOX_VIEW_PREF,
        "FEATURE_CALLOUT_[0-9]"
      )} && ${matchIncompleteTargeting(FIREFOX_VIEW_PREF)}`,
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
            anchors: [
              {
                selector: "#tab-pickup-container",
                arrow_position: "top",
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
              page_event_listeners: [
                {
                  params: {
                    type: "toggle",
                    selectors: "#tab-pickup-container",
                  },
                  action: { reposition: true },
                },
              ],
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
      id: "PDFJS_FEATURE_TOUR_A",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        tour_pref_name: PDFJS_PREF,
        screens: [
          {
            id: "FEATURE_CALLOUT_1_A",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "51px" },
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
          {
            id: "FEATURE_CALLOUT_2_A",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "23px" },
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
      targeting: `source == "open" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_[0-9]_A"
      )} && ${matchIncompleteTargeting(PDFJS_PREF)}`,
      trigger: { id: "pdfJsFeatureCalloutCheck" },
    },
    {
      id: "PDFJS_FEATURE_TOUR_B",
      template: "feature_callout",
      content: {
        id: "PDFJS_FEATURE_TOUR",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        tour_pref_name: PDFJS_PREF,
        screens: [
          {
            id: "FEATURE_CALLOUT_1_B",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "51px" },
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
          {
            id: "FEATURE_CALLOUT_2_B",
            anchors: [
              {
                selector: "hbox#browser",
                arrow_position: "top-end",
                absolute_position: { top: "43px", right: "23px" },
              },
            ],
            content: {
              position: "callout",
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
                style: "secondary",
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
      targeting: `source == "open" && ${matchCurrentScreenTargeting(
        PDFJS_PREF,
        "FEATURE_CALLOUT_[0-9]_B"
      )} && ${matchIncompleteTargeting(PDFJS_PREF)}`,
      trigger: { id: "pdfJsFeatureCalloutCheck" },
    },
    {
      // "Callout 1" in the Fakespot Figma spec
      id: "FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              title_logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/shopping.svg",
                alignment: "top",
              },
              title: {
                string_id: "shopping-callout-closed-opted-in-subtitle",
                marginInline: "3px 40px",
                fontWeight: "inherit",
              },
              dismiss_button: {
                action: { dismiss: true },
                size: "small",
                marginBlock: "24px 0",
                marginInline: "0 24px",
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
            },
          },
        ],
      },
      priority: 1,
      // Auto-open feature flag is not enabled; User is opted in; First time closing sidebar; Has not seen either on-closed callout before; Has not opted out of CFRs.
      targeting: `isSidebarClosing && 'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue != true && 'browser.shopping.experience2023.optedIn' | preferenceValue == 1 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false && !messageImpressions.FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT|length && !messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT|length`,
      trigger: { id: "shoppingProductPageWithSidebarClosed" },
      frequency: { lifetime: 1 },
    },
    {
      // "Callout 3" in the Fakespot Figma spec
      id: "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              title_logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/shopping.svg",
              },
              title: {
                string_id: "shopping-callout-closed-not-opted-in-title",
                marginInline: "3px 40px",
              },
              subtitle: {
                string_id: "shopping-callout-closed-not-opted-in-subtitle",
              },
              dismiss_button: {
                action: { dismiss: true },
                size: "small",
                marginBlock: "24px 0",
                marginInline: "0 24px",
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
            },
          },
        ],
      },
      priority: 1,
      // Auto-open feature flag is not enabled; User is not opted in; First time closing sidebar; Has not seen either on-closed callout before; Has not opted out of CFRs.
      targeting: `isSidebarClosing && 'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue != true && 'browser.shopping.experience2023.optedIn' | preferenceValue != 1 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false && !messageImpressions.FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT|length && !messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT|length`,
      trigger: { id: "shoppingProductPageWithSidebarClosed" },
      frequency: { lifetime: 1 },
    },
    {
      // "callout 2" in the Fakespot Figma spec
      id: "FAKESPOT_CALLOUT_PDP_OPTED_IN_DEFAULT",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_PDP_OPTED_IN_DEFAULT",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_PDP_OPTED_IN_DEFAULT",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              title: { string_id: "shopping-callout-pdp-opted-in-title" },
              subtitle: { string_id: "shopping-callout-pdp-opted-in-subtitle" },
              logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/ratingLight.avif",
                darkModeImageURL:
                  "chrome://browser/content/shopping/assets/ratingDark.avif",
                height: "216px",
              },
              dismiss_button: {
                action: { dismiss: true },
                size: "small",
                marginBlock: "24px 0",
                marginInline: "0 24px",
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
            },
          },
        ],
      },
      priority: 1,
      // Auto-open feature flag is not enabled; User is opted in; Has not opted out of CFRs; Has seen either on-closed callout before, but not within the last 24hrs or in this session.
      targeting: `!isSidebarClosing && 'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue != true && 'browser.shopping.experience2023.optedIn' | preferenceValue == 1 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false && ((currentDate | date - messageImpressions.FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT[messageImpressions.FAKESPOT_CALLOUT_CLOSED_OPTED_IN_DEFAULT | length - 1] | date) / 3600000 > 24 || (currentDate | date - messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT[messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_DEFAULT | length - 1] | date) / 3600000 > 24)`,
      trigger: { id: "shoppingProductPageWithSidebarClosed" },
      frequency: { lifetime: 1 },
    },
    {
      // "Callout 1" in the Fakespot Figma spec, but
      // targeting not opted-in users only for rediscoverability experiment 2.
      id: "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_AUTO_OPEN",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_AUTO_OPEN",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_AUTO_OPEN",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              width: "401px",
              title: {
                string_id: "shopping-callout-closed-not-opted-in-revised-title",
              },
              subtitle: {
                string_id:
                  "shopping-callout-closed-not-opted-in-revised-subtitle",
                letterSpacing: "0",
              },
              logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/priceTagButtonCallout.svg",
                height: "214px",
              },
              dismiss_button: {
                action: { dismiss: true },
                size: "small",
                marginBlock: "28px 0",
                marginInline: "0 28px",
              },
              primary_button: {
                label: {
                  string_id:
                    "shopping-callout-closed-not-opted-in-revised-button",
                  marginBlock: "0 -8px",
                },
                style: "secondary",
                action: {
                  dismiss: true,
                },
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
            },
          },
        ],
      },
      priority: 1,
      // Auto-open feature flag is enabled; User is not opted in; First time closing sidebar; Has not opted out of CFRs.
      targeting: `isSidebarClosing && 'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue == true && 'browser.shopping.experience2023.optedIn' | preferenceValue != 1 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false`,
      trigger: { id: "shoppingProductPageWithSidebarClosed" },
      frequency: { lifetime: 1 },
      skip_in_tests:
        "not tested in automation and might pop up unexpectedly during review checker tests",
    },
    {
      // "Callout 3" in the Fakespot Figma spec, but
      // displayed if auto-open version of "callout 1" was seen already and 24 hours have passed.
      id: "FAKESPOT_CALLOUT_PDP_NOT_OPTED_IN_REMINDER",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_PDP_NOT_OPTED_IN_REMINDER",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_PDP_NOT_OPTED_IN_REMINDER",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              width: "401px",
              title: {
                string_id: "shopping-callout-not-opted-in-reminder-title",
                fontSize: "20px",
                letterSpacing: "0",
              },
              subtitle: {
                string_id: "shopping-callout-not-opted-in-reminder-subtitle",
                letterSpacing: "0",
              },
              logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/reviewsVisualCallout.svg",
                alt: {
                  string_id: "shopping-callout-not-opted-in-reminder-img-alt",
                },
                height: "214px",
              },
              dismiss_button: {
                action: {
                  type: "MULTI_ACTION",
                  collectSelect: true,
                  data: {
                    actions: [],
                  },
                  dismiss: true,
                },
                size: "small",
                marginBlock: "28px 0",
                marginInline: "0 28px",
              },
              primary_button: {
                label: {
                  string_id:
                    "shopping-callout-not-opted-in-reminder-close-button",
                  marginBlock: "0 -8px",
                },
                style: "secondary",
                action: {
                  type: "MULTI_ACTION",
                  collectSelect: true,
                  data: {
                    actions: [],
                  },
                  dismiss: true,
                },
              },
              secondary_button: {
                label: {
                  string_id:
                    "shopping-callout-not-opted-in-reminder-open-button",
                  marginBlock: "0 -8px",
                },
                style: "primary",
                action: {
                  type: "MULTI_ACTION",
                  collectSelect: true,
                  data: {
                    actions: [
                      {
                        type: "SET_PREF",
                        data: {
                          pref: {
                            name: "browser.shopping.experience2023.active",
                            value: true,
                          },
                        },
                      },
                    ],
                  },
                  dismiss: true,
                },
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
              tiles: {
                type: "multiselect",
                style: {
                  flexDirection: "column",
                  alignItems: "flex-start",
                },
                data: [
                  {
                    id: "checkbox-dont-show-again",
                    type: "checkbox",
                    defaultValue: false,
                    style: {
                      alignItems: "center",
                    },
                    label: {
                      string_id:
                        "shopping-callout-not-opted-in-reminder-ignore-checkbox",
                    },
                    icon: {
                      style: {
                        width: "16px",
                        height: "16px",
                        marginInline: "0 8px",
                      },
                    },
                    action: {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "messaging-system-action.shopping-callouts-1-block",
                          value: true,
                        },
                      },
                    },
                  },
                ],
              },
            },
          },
        ],
      },
      priority: 2,
      // Auto-open feature flag is enabled; User is not opted in; Has not opted out of CFRs; Has seen callout 1 before, but not within the last 5 days.
      targeting:
        "!isSidebarClosing && 'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue == true && 'browser.shopping.experience2023.optedIn' | preferenceValue == 0 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false && !'messaging-system-action.shopping-callouts-1-block' | preferenceValue && (currentDate | date - messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_AUTO_OPEN[messageImpressions.FAKESPOT_CALLOUT_CLOSED_NOT_OPTED_IN_AUTO_OPEN | length - 1] | date) / 3600000 > 24",
      trigger: {
        id: "shoppingProductPageWithSidebarClosed",
      },
      frequency: {
        custom: [
          {
            cap: 1,
            period: 432000000,
          },
        ],
        lifetime: 3,
      },
      skip_in_tests:
        "not tested in automation and might pop up unexpectedly during review checker tests",
    },
    {
      // "Callout 4" in the Fakespot Figma spec, for rediscoverability experiment 2.
      id: "FAKESPOT_CALLOUT_DISABLED_AUTO_OPEN",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_DISABLED_AUTO_OPEN",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_DISABLED_AUTO_OPEN",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              width: "401px",
              title: {
                string_id: "shopping-callout-disabled-auto-open-title",
              },
              subtitle: {
                string_id: "shopping-callout-disabled-auto-open-subtitle",
                letterSpacing: "0",
              },
              logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/priceTagButtonCallout.svg",
                height: "214px",
              },
              dismiss_button: {
                action: { dismiss: true },
                size: "small",
                marginBlock: "28px 0",
                marginInline: "0 28px",
              },
              primary_button: {
                label: {
                  string_id: "shopping-callout-disabled-auto-open-button",
                  marginBlock: "0 -8px",
                },
                style: "secondary",
                action: {
                  dismiss: true,
                },
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
            },
          },
        ],
      },
      priority: 1,
      // Auto-open feature flag is enabled; User disabled auto-open behavior; User is opted in; Has not opted out of CFRs.
      targeting: `'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue == true && 'browser.shopping.experience2023.autoOpen.userEnabled' | preferenceValue == false && 'browser.shopping.experience2023.optedIn' | preferenceValue == 1 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false`,
      trigger: {
        id: "preferenceObserver",
        params: ["browser.shopping.experience2023.autoOpen.userEnabled"],
      },
      frequency: { lifetime: 1 },
      skip_in_tests:
        "not tested in automation and might pop up unexpectedly during review checker tests",
    },
    {
      // "Callout 5" in the Fakespot Figma spec, for rediscoverability experiment 2.
      id: "FAKESPOT_CALLOUT_OPTED_OUT_AUTO_OPEN",
      template: "feature_callout",
      content: {
        id: "FAKESPOT_CALLOUT_OPTED_OUT_AUTO_OPEN",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "FAKESPOT_CALLOUT_OPTED_OUT_AUTO_OPEN",
            anchors: [
              {
                selector: "#shopping-sidebar-button",
                panel_position: {
                  anchor_attachment: "bottomcenter",
                  callout_attachment: "topright",
                },
                no_open_on_anchor: true,
              },
            ],
            content: {
              position: "callout",
              width: "401px",
              title: {
                string_id: "shopping-callout-opted-out-title",
              },
              subtitle: {
                string_id: "shopping-callout-opted-out-subtitle",
                letterSpacing: "0",
              },
              logo: {
                imageURL:
                  "chrome://browser/content/shopping/assets/priceTagButtonCallout.svg",
                height: "214px",
              },
              dismiss_button: {
                action: { dismiss: true },
                size: "small",
                marginBlock: "28px 0",
                marginInline: "0 28px",
              },
              primary_button: {
                label: {
                  string_id: "shopping-callout-opted-out-button",
                  marginBlock: "0 -8px",
                },
                style: "secondary",
                action: {
                  dismiss: true,
                },
              },
              page_event_listeners: [
                {
                  params: {
                    type: "click",
                    selectors: "#shopping-sidebar-button",
                  },
                  action: { dismiss: true },
                },
              ],
            },
          },
        ],
      },
      priority: 1,
      // Auto-open feature flag is enabled; User has opted out; Has not opted out of CFRs.
      targeting: `'browser.shopping.experience2023.autoOpen.enabled' | preferenceValue == true && 'browser.shopping.experience2023.optedIn' | preferenceValue == 2 && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false`,
      trigger: {
        id: "preferenceObserver",
        params: ["browser.shopping.experience2023.optedIn"],
      },
      frequency: { lifetime: 1 },
      skip_in_tests:
        "not tested in automation and might pop up unexpectedly during review checker tests",
    },

    // cookie banner reduction onboarding
    {
      id: "CFR_COOKIEBANNER",
      groups: ["cfr"],
      template: "feature_callout",
      content: {
        id: "CFR_COOKIEBANNER",
        template: "multistage",
        backdrop: "transparent",
        transitions: false,
        disableHistoryUpdates: true,
        screens: [
          {
            id: "COOKIEBANNER_CALLOUT",
            anchors: [
              {
                selector: "#tracking-protection-icon-container",
                panel_position: {
                  callout_attachment: "topleft",
                  anchor_attachment: "bottomcenter",
                },
              },
            ],
            content: {
              position: "callout",
              autohide: true,
              title: {
                string_id: "cookie-banner-blocker-onboarding-header",
                paddingInline: "12px 0",
              },
              subtitle: {
                string_id: "cookie-banner-blocker-onboarding-body",
                paddingInline: "34px 0",
              },
              title_logo: {
                alignment: "top",
                height: "20px",
                width: "20px",
                imageURL:
                  "chrome://browser/skin/controlcenter/3rdpartycookies-blocked.svg",
              },
              dismiss_button: {
                size: "small",
                action: { dismiss: true },
              },
              additional_button: {
                label: {
                  string_id: "cookie-banner-blocker-onboarding-learn-more",
                  marginInline: "34px 0",
                },
                style: "link",
                alignment: "start",
                action: {
                  type: "OPEN_URL",
                  data: {
                    args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cookie-banner-reduction",
                    where: "tabshifted",
                  },
                },
              },
            },
          },
        ],
      },
      frequency: {
        lifetime: 1,
      },
      skip_in_tests: "it's not tested in automation",
      trigger: {
        id: "cookieBannerHandled",
      },
      targeting: `'cookiebanners.ui.desktop.enabled'|preferenceValue == true && 'cookiebanners.ui.desktop.showCallout'|preferenceValue == true && 'browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features' | preferenceValue != false`,
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
