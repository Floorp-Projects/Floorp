/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const FIREFOX_VERSION = parseInt(Services.appinfo.version.match(/\d+/), 10);
const TWO_DAYS = 2 * 24 * 3600 * 1000;

const MESSAGES = () => [
  {
    id: "SIMPLE_FXA_BOOKMARK_TEST_FLUENT",
    template: "fxa_bookmark_panel",
    content: {
      title: { string_id: "cfr-doorhanger-bookmark-fxa-header" },
      text: { string_id: "cfr-doorhanger-bookmark-fxa-body" },
      cta: { string_id: "cfr-doorhanger-bookmark-fxa-link-text" },
      color: "white",
      background_color_1: "#7d31ae",
      background_color_2: "#5033be",
      info_icon: {
        tooltiptext: {
          string_id: "cfr-doorhanger-bookmark-fxa-info-icon-tooltip",
        },
      },
      close_button: {
        tooltiptext: {
          string_id: "cfr-doorhanger-bookmark-fxa-close-btn-tooltip",
        },
      },
    },
    trigger: { id: "bookmark-panel" },
  },
  {
    id: "SIMPLE_FXA_BOOKMARK_TEST_NON_FLUENT",
    template: "fxa_bookmark_panel",
    content: {
      title: "Bookmark Message Title",
      text: "Bookmark Message Body",
      cta: "Sync bookmarks now",
      color: "white",
      background_color_1: "#7d31ae",
      background_color_2: "#5033be",
      info_icon: {
        tooltiptext: "Toggle tooltip",
      },
      close_button: {
        tooltiptext: "Close tooltip",
      },
    },
    trigger: { id: "bookmark-panel" },
  },
  {
    id: "FXA_ACCOUNTS_BADGE",
    template: "toolbar_badge",
    content: {
      target: "fxa-toolbar-menu-button",
    },
    // Never accessed the FxA panel && doesn't use Firefox sync & has FxA enabled
    targeting: `!hasAccessedFxAPanel && !usesFirefoxSync && isFxAEnabled == true`,
    trigger: { id: "toolbarBadgeUpdate" },
  },
  {
    id: "WNP_THANK_YOU",
    template: "update_action",
    content: {
      action: {
        id: "moments-wnp",
        data: {
          url:
            "https://www.mozilla.org/%LOCALE%/etc/firefox/retention/thank-you-a/",
          expireDelta: TWO_DAYS,
        },
      },
    },
    trigger: { id: "momentsUpdate" },
  },
  {
    id: `WHATS_NEW_BADGE_${FIREFOX_VERSION}`,
    template: "toolbar_badge",
    content: {
      // delay: 5 * 3600 * 1000,
      delay: 5000,
      target: "whats-new-menu-button",
      action: { id: "show-whatsnew-button" },
    },
    priority: 1,
    trigger: { id: "toolbarBadgeUpdate" },
    frequency: {
      // Makes it so that we track impressions for this message while at the
      // same time it can have unlimited impressions
      lifetime: Infinity,
    },
    // Never saw this message or saw it in the past 4 days or more recent
    targeting: `isWhatsNewPanelEnabled &&
      (earliestFirefoxVersion && firefoxVersion > earliestFirefoxVersion) &&
        messageImpressions[.id == 'WHATS_NEW_BADGE_${FIREFOX_VERSION}']|length == 0 ||
      (messageImpressions[.id == 'WHATS_NEW_BADGE_${FIREFOX_VERSION}']|length >= 1 &&
        currentDate|date - messageImpressions[.id == 'WHATS_NEW_BADGE_${FIREFOX_VERSION}'][0] <= 4 * 24 * 3600 * 1000)`,
  },
];

const PanelTestProvider = {
  getMessages() {
    return MESSAGES().map(message => ({
      ...message,
      targeting: `providerCohorts.panel_local_testing == "SHOW_TEST"`,
    }));
  },
};
this.PanelTestProvider = PanelTestProvider;

const EXPORTED_SYMBOLS = ["PanelTestProvider"];
