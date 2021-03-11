/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

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
    id: "WHATS_NEW_FINGERPRINTER_COUNTER_72",
    template: "whatsnew_panel_message",
    order: 6,
    content: {
      bucket_id: "WHATS_NEW_72",
      published_date: 1574776601000,
      layout: "tracking-protections",
      layout_title_content_variable: "fingerprinterCount",
      title: { string_id: "cfr-whatsnew-fingerprinter-counter-header" },
      subtitle: { string_id: "cfr-whatsnew-tracking-blocked-subtitle" },
      icon_url:
        "chrome://activity-stream/content/data/content/assets/protection-report-icon.png",
      icon_alt: "",
      body: { string_id: "cfr-whatsnew-fingerprinter-counter-body" },
      link_text: { string_id: "cfr-whatsnew-tracking-blocked-link-text" },
      cta_url: "",
      cta_type: "OPEN_PROTECTION_REPORT",
    },
    targeting: `firefoxVersion >= 72`,
    trigger: { id: "whatsNewPanelOpened" },
  },
  {
    id: "WHATS_NEW_FINGERPRINTER_COUNTER_ALT",
    template: "whatsnew_panel_message",
    order: 6,
    content: {
      bucket_id: "WHATS_NEW_72",
      published_date: 1574776601000,
      title: { string_id: "cfr-whatsnew-fingerprinter-counter-header-alt" },
      icon_url:
        "chrome://activity-stream/content/data/content/assets/protection-report-icon.png",
      icon_alt: "",
      body: { string_id: "cfr-whatsnew-fingerprinter-counter-body-alt" },
      link_text: { string_id: "cfr-whatsnew-tracking-blocked-link-text" },
      cta_url: "",
      cta_type: "OPEN_PROTECTION_REPORT",
    },
    targeting: `firefoxVersion >= 72`,
    trigger: { id: "whatsNewPanelOpened" },
  },
  {
    id: "WHATS_NEW_70_1",
    template: "whatsnew_panel_message",
    order: 3,
    content: {
      bucket_id: "WHATS_NEW_70_1",
      published_date: 1560969794394,
      title: "Protection Is Our Focus",
      icon_url:
        "chrome://activity-stream/content/data/content/assets/whatsnew-send-icon.png",
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
      bucket_id: "WHATS_NEW_70_1",
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
    id: "WHATS_NEW_SEARCH_SHORTCUTS_84",
    template: "whatsnew_panel_message",
    order: 2,
    content: {
      bucket_id: "WHATS_NEW_SEARCH_SHORTCUTS_84",
      published_date: 1560969794394,
      title: {
        string_id: "cfr-whatsnew-search-shortcuts-header",
      },
      icon_url: "chrome://global/skin/icons/success.svg",
      icon_alt: {
        string_id: "cfr-whatsnew-searchbar-icon-alt-text",
      },
      body: {
        string_id: "cfr-whatsnew-search-shortcuts-body",
      },
      cta_url:
        "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/search-shortcuts",
      cta_type: "OPEN_URL",
      link_text: {
        string_id: "cfr-whatsnew-pip-cta",
      },
    },
    targeting: "firefoxVersion >= 84",
    trigger: {
      id: "whatsNewPanelOpened",
    },
  },
  {
    id: "WHATS_NEW_PIONEER_82",
    template: "whatsnew_panel_message",
    order: 1,
    content: {
      bucket_id: "WHATS_NEW_PIONEER_82",
      published_date: 1603152000000,
      title: "Put your data to work for a better internet",
      icon_url: "",
      icon_alt: "",
      body:
        "Contribute your data to Mozilla's Pioneer program to help researchers understand pressing technology issues like misinformation, data privacy, and ethical AI.",
      cta_url: "pioneer",
      cta_where: "tab",
      cta_type: "OPEN_ABOUT_PAGE",
      link_text: "Join Pioneer",
    },
    targeting: "firefoxVersion >= 82",
    trigger: {
      id: "whatsNewPanelOpened",
    },
  },
  {
    id: "WHATS_NEW_MEDIA_SESSION_82",
    template: "whatsnew_panel_message",
    order: 3,
    content: {
      bucket_id: "WHATS_NEW_MEDIA_SESSION_82",
      published_date: 1603152000000,
      title: {
        string_id: "cfr-whatsnew-media-keys-header",
      },
      body: {
        string_id: "cfr-whatsnew-media-keys-body",
      },
      cta_url:
        "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/media-keyboard-control",
      cta_type: "OPEN_URL",
      link_text: {
        string_id: "cfr-whatsnew-media-keys-button",
      },
    },
    targeting: "firefoxVersion >= 82",
    trigger: {
      id: "whatsNewPanelOpened",
    },
  },
  {
    id: "WHATS_NEW_69_1",
    template: "whatsnew_panel_message",
    order: 1,
    content: {
      bucket_id: "WHATS_NEW_69_1",
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
    id: "BOOKMARK_CFR",
    template: "cfr_doorhanger",
    groups: ["cfr"],
    content: {
      layout: "icon_and_message",
      category: "cfrFeatures",
      notification_text: { string_id: "cfr-doorhanger-feature-notification" },
      heading_text: { string_id: "cfr-doorhanger-sync-bookmarks-header" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: "https://example.com",
      },
      text: { string_id: "cfr-doorhanger-sync-bookmarks-body" },
      icon: "chrome://branding/content/icon64.png",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-sync-bookmarks-ok-button" },
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: { category: "sync" },
          },
        },
        secondary: [
          {
            label: { string_id: "cfr-doorhanger-extension-cancel-button" },
            action: { type: "CANCEL" },
          },
          {
            label: {
              string_id: "cfr-doorhanger-extension-never-show-recommendation",
            },
          },
          {
            label: {
              string_id: "cfr-doorhanger-extension-manage-settings-button",
            },
            action: {
              type: "OPEN_PREFERENCES_PAGE",
              data: { category: "general-cfrfeatures" },
            },
          },
        ],
      },
    },
    targeting: "true",
    trigger: {
      id: "openBookmarkedURL",
    },
  },
  {
    id: "PERSONALIZED_CFR_MESSAGE",
    template: "cfr_doorhanger",
    groups: ["cfr"],
    content: {
      layout: "icon_and_message",
      category: "cfrFeatures",
      notification_text: "Personalized CFR Recommendation",
      heading_text: { string_id: "cfr-doorhanger-firefox-send-header" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: "https://example.com",
      },
      text: { string_id: "cfr-doorhanger-firefox-send-body" },
      icon: "chrome://branding/content/icon64.png",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-firefox-send-ok-button" },
          action: {
            type: "OPEN_URL",
            data: {
              args:
                "https://send.firefox.com/login/?utm_source=activity-stream&entrypoint=activity-stream-cfr-pdf",
              where: "tabshifted",
            },
          },
        },
        secondary: [
          {
            label: { string_id: "cfr-doorhanger-extension-cancel-button" },
            action: { type: "CANCEL" },
          },
          {
            label: {
              string_id: "cfr-doorhanger-extension-never-show-recommendation",
            },
          },
          {
            label: {
              string_id: "cfr-doorhanger-extension-manage-settings-button",
            },
            action: {
              type: "OPEN_PREFERENCES_PAGE",
              data: { category: "general-cfrfeatures" },
            },
          },
        ],
      },
    },
    targeting: "scores.PERSONALIZED_CFR_MESSAGE.score > scoreThreshold",
    trigger: {
      id: "openURL",
      patterns: ["*://*/*.pdf"],
    },
  },
];

const PanelTestProvider = {
  getMessages() {
    return Promise.resolve(
      MESSAGES().map(message => ({
        ...message,
        targeting: `providerCohorts.panel_local_testing == "SHOW_TEST"`,
      }))
    );
  },
};
this.PanelTestProvider = PanelTestProvider;

const EXPORTED_SYMBOLS = ["PanelTestProvider"];
