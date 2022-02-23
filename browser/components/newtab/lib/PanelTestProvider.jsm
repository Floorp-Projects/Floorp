/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TWO_DAYS = 2 * 24 * 3600 * 1000;

const MESSAGES = () => [
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
    id: "WHATS_NEW_FINGERPRINTER_COUNTER_ALT",
    template: "whatsnew_panel_message",
    order: 6,
    content: {
      bucket_id: "WHATS_NEW_72",
      published_date: 1574776601000,
      title: "Title",
      icon_url:
        "chrome://activity-stream/content/data/content/assets/protection-report-icon.png",
      icon_alt: { string_id: "cfr-badge-reader-label-newfeature" },
      body: "Message body",
      link_text: "Click here",
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
      title: "Title",
      icon_url: "chrome://global/skin/icons/check.svg",
      icon_alt: "",
      body: "Message content",
      cta_url:
        "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/search-shortcuts",
      cta_type: "OPEN_URL",
      link_text: "Click here",
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
      title: "Title",
      body: "Message content",
      cta_url:
        "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/media-keyboard-control",
      cta_type: "OPEN_URL",
      link_text: "Click here",
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
    id: "PERSONALIZED_CFR_MESSAGE",
    template: "cfr_doorhanger",
    groups: ["cfr"],
    content: {
      layout: "icon_and_message",
      category: "cfrFeatures",
      notification_text: "Personalized CFR Recommendation",
      heading_text: { string_id: "cfr-doorhanger-bookmark-fxa-header" },
      info_icon: {
        label: {
          attributes: {
            tooltiptext: { string_id: "cfr-doorhanger-fxa-close-btn-tooltip" },
          },
        },
        sumo_path: "https://example.com",
      },
      text: { string_id: "cfr-doorhanger-bookmark-fxa-body" },
      icon: "chrome://branding/content/icon64.png",
      icon_class: "cfr-doorhanger-large-icon",
      persistent_doorhanger: true,
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-milestone-ok-button" },
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
  {
    id: "SPOTLIGHT_MESSAGE_93",
    template: "spotlight",
    groups: ["panel-test-provider"],
    content: {
      template: "logo-and-content",
      logoImageURL: "chrome://browser/content/logos/vpn-promo-logo.svg",
      body: {
        title: {
          label: {
            string_id: "spotlight-public-wifi-vpn-header",
          },
        },
        text: {
          label: {
            string_id: "spotlight-public-wifi-vpn-body",
          },
        },
        primary: {
          label: {
            string_id: "spotlight-public-wifi-vpn-primary-button",
          },
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/en-US/products/vpn/",
              where: "tabshifted",
            },
          },
        },
        secondary: {
          label: {
            string_id: "spotlight-public-wifi-vpn-link",
          },
          action: {
            type: "CANCEL",
          },
        },
      },
    },
    frequency: { lifetime: 3 },
    trigger: { id: "defaultBrowserCheck" },
  },
  {
    id: "TCP_SPOTLIGHT_MESSAGE_95",
    groups: ["panel-test-provider"],
    template: "spotlight",
    content: {
      template: "logo-and-content",
      logo: {
        imageURL: "chrome://branding/content/about-logo@2x.png",
        size: "64px",
      },
      body: {
        title: {
          label: {
            string_id: "spotlight-total-cookie-protection-header",
          },
          size: "24px",
        },
        text: {
          label: {
            string_id: "spotlight-total-cookie-protection-body",
          },
          size: "20px",
        },
        primary: {
          label: {
            string_id: "spotlight-total-cookie-protection-primary-button",
          },
          action: {
            type: "ENABLE_TOTAL_COOKIE_PROTECTION",
          },
        },
        secondary: {
          label: {
            string_id: "spotlight-total-cookie-protection-secondary-button",
          },
          action: {
            type: "ENABLE_TOTAL_COOKIE_PROTECTION_SECTION_AND_OPT_OUT",
          },
        },
      },
      extra: {
        expanded: {
          label: {
            string_id: "spotlight-total-cookie-protection-expanded",
          },
          size: "13px",
        },
      },
    },
    frequency: { lifetime: 3 },
    trigger: { id: "defaultBrowserCheck" },
  },
  {
    id: "PB_NEWTAB_VPN_PROMO",
    template: "pb_newtab",
    content: {
      promoEnabled: true,
      infoEnabled: true,
      infoIcon: "",
      infoTitle: "",
      infoBody: "fluent:about-private-browsing-info-description-private-window",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitleEnabled: false,
      promoLinkType: "button",
      promoLinkText: "fluent:about-private-browsing-prominent-cta",
      promoSectionStyle: "below-search",
      promoHeader: "fluent:about-private-browsing-get-privacy",
      promoTitle: "fluent:about-private-browsing-hide-activity-1",
      promoTitleEnabled: true,
      promoImageLarge: "chrome://browser/content/assets/moz-vpn.svg",
    },
    targeting: "region != 'CN' && !hasActiveEnterprisePolicies",
    frequency: { lifetime: 3 },
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
