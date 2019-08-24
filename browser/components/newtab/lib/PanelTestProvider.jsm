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
        (!messageImpressions['WHATS_NEW_BADGE_${FIREFOX_VERSION}'] ||
      (messageImpressions['WHATS_NEW_BADGE_${FIREFOX_VERSION}']|length >= 1 &&
        currentDate|date - messageImpressions['WHATS_NEW_BADGE_${FIREFOX_VERSION}'][0] <= 4 * 24 * 3600 * 1000))`,
  },
  {
    id: "WHATS_NEW_70_1",
    template: "whatsnew_panel_message",
    order: 3,
    content: {
      published_date: 1560969794394,
      title: "Protection Is Our Focus",
      icon_url:
        "resource://activity-stream/data/content/assets/whatsnew-send-icon.png",
      icon_alt: "Firefox Send Logo",
      body:
        "The New Enhanced Tracking Protection, gives you the best level of protection and performance. Discover how this version is the safest version of firefox ever made.",
      cta_url: "https://blog.mozilla.org/",
      cta_type: "OPEN_URL",
    },
    targeting: `firefoxVersion > 69`,
    trigger: { id: "whatsNewPanelOpened" },
  },
  {
    id: "WHATS_NEW_70_2",
    template: "whatsnew_panel_message",
    order: 1,
    content: {
      published_date: 1560969794394,
      title: "Another thing new in Firefox 70",
      body:
        "The New Enhanced Tracking Protection, gives you the best level of protection and performance. Discover how this version is the safest version of firefox ever made.",
      link_text: "Learn more on our blog",
      cta_url: "https://blog.mozilla.org/",
      cta_type: "OPEN_URL",
    },
    targeting: `firefoxVersion > 69`,
    trigger: { id: "whatsNewPanelOpened" },
  },
  {
    id: "WHATS_NEW_69_1",
    template: "whatsnew_panel_message",
    order: 1,
    content: {
      published_date: 1557346235089,
      title: "Something new in Firefox 69",
      body:
        "The New Enhanced Tracking Protection, gives you the best level of protection and performance. Discover how this version is the safest version of firefox ever made.",
      link_text: "Learn more on our blog",
      cta_url: "https://blog.mozilla.org/",
      cta_type: "OPEN_URL",
    },
    targeting: `firefoxVersion > 68`,
    trigger: { id: "whatsNewPanelOpened" },
  },
  {
    id: "WHATS_NEW_70_3",
    template: "whatsnew_panel_message",
    order: 2,
    content: {
      published_date: 1560969794394,
      layout: "tracking-protections",
      title: { string_id: "cfr-whatsnew-tracking-blocked-title" },
      subtitle: { string_id: "cfr-whatsnew-tracking-blocked-subtitle" },
      icon_url:
        "resource://activity-stream/data/content/assets/protection-report-icon.png",
      icon_alt: "Protection Report icon",
      body: { string_id: "cfr-whatsnew-tracking-protect-body" },
      link_text: { string_id: "cfr-whatsnew-tracking-blocked-link-text" },
      cta_url: "protections",
      cta_type: "OPEN_ABOUT_PAGE",
    },
    targeting: `firefoxVersion > 69 && totalBlockedCount > 0`,
    trigger: { id: "whatsNewPanelOpened" },
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
