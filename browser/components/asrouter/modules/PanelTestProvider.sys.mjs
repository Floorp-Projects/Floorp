/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TWO_DAYS = 2 * 24 * 3600 * 1000;

const MESSAGES = () => [
  {
    id: "WNP_THANK_YOU",
    template: "update_action",
    content: {
      action: {
        id: "moments-wnp",
        data: {
          url: "https://www.mozilla.org/%LOCALE%/etc/firefox/retention/thank-you-a/",
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
      cta_url: "about:blank",
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
      body: "The New Enhanced Tracking Protection, gives you the best level of protection and performance. Discover how this version is the safest version of firefox ever made.",
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
      body: "The New Enhanced Tracking Protection, gives you the best level of protection and performance. Discover how this version is the safest version of firefox ever made.",
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
      body: "Contribute your data to Mozilla's Pioneer program to help researchers understand pressing technology issues like misinformation, data privacy, and ethical AI.",
      cta_url: "about:blank",
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
      body: "The New Enhanced Tracking Protection, gives you the best level of protection and performance. Discover how this version is the safest version of firefox ever made.",
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
      bucket_id: "PERSONALIZED_CFR_MESSAGE",
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
              args: "https://send.firefox.com/login/?utm_source=activity-stream&entrypoint=activity-stream-cfr-pdf",
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
    id: "MULTISTAGE_SPOTLIGHT_MESSAGE",
    groups: ["panel-test-provider"],
    template: "spotlight",
    content: {
      id: "MULTISTAGE_SPOTLIGHT_MESSAGE",
      template: "multistage",
      backdrop: "transparent",
      transitions: true,
      screens: [
        {
          id: "AW_PIN_FIREFOX",
          content: {
            has_noodles: true,
            title: {
              string_id: "onboarding-easy-setup-security-and-privacy-title",
            },
            logo: {
              imageURL: "chrome://browser/content/callout-tab-pickup.svg",
              darkModeImageURL:
                "chrome://browser/content/callout-tab-pickup-dark.svg",
              reducedMotionImageURL:
                "chrome://activity-stream/content/data/content/assets/glyph-pin-16.svg",
              darkModeReducedMotionImageURL:
                "chrome://activity-stream/content/data/content/assets/firefox.svg",
              alt: "sample alt text",
            },
            hero_text: {
              string_id: "fx100-thank-you-hero-text",
            },
            primary_button: {
              label: {
                string_id: "mr2022-onboarding-pin-primary-button-label",
              },
              action: {
                navigate: true,
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
            },
            secondary_button: {
              label: {
                string_id: "onboarding-not-now-button-label",
              },
              action: {
                navigate: true,
              },
            },
            dismiss_button: {
              action: {
                dismiss: true,
              },
            },
          },
        },
        {
          id: "AW_SET_DEFAULT",
          content: {
            has_noodles: true,
            logo: {
              imageURL: "chrome://browser/content/logos/vpn-promo-logo.svg",
              height: "100px",
            },
            title: {
              fontSize: "36px",
              fontWeight: 276,
              string_id: "mr2022-onboarding-set-default-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-set-default-subtitle",
            },
            primary_button: {
              label: {
                string_id: "mr2022-onboarding-set-default-primary-button-label",
              },
              action: {
                navigate: true,
                type: "SET_DEFAULT_BROWSER",
              },
            },
            secondary_button: {
              label: {
                string_id: "onboarding-not-now-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
        {
          id: "BACKGROUND_IMAGE",
          content: {
            background: "#000",
            text_color: "light",
            progress_bar: true,
            logo: {
              imageURL:
                "https://firefox-settings-attachments.cdn.mozilla.net/main-workspace/ms-images/a3c640c8-7594-4bb2-bc18-8b4744f3aaf2.gif",
            },
            title: "A dialog with a background",
            subtitle:
              "The text color is configurable and a progress bar style step indicator is used",
            primary_button: {
              label: "Continue",
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "onboarding-not-now-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
        {
          id: "BACKGROUND_COLOR",
          content: {
            background: "white",
            progress_bar: true,
            logo: {
              height: "200px",
              imageURL: "",
            },
            title: {
              fontSize: "36px",
              fontWeight: 276,
              raw: "Peace of mind.",
            },
            title_style: "fancy shine",
            text_color: "dark",
            subtitle: "Using progress bar style step indicator",
            primary_button: {
              label: "Continue",
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "onboarding-not-now-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
      ],
    },
    frequency: { lifetime: 3 },
    trigger: { id: "defaultBrowserCheck" },
  },
  {
    id: "PB_FOCUS_PROMO",
    groups: ["panel-test-provider"],
    template: "spotlight",
    content: {
      template: "multistage",
      backdrop: "transparent",
      screens: [
        {
          id: "PBM_FIREFOX_FOCUS",
          order: 0,
          content: {
            logo: {
              imageURL: "chrome://browser/content/assets/focus-logo.svg",
              height: "48px",
            },
            title: {
              string_id: "spotlight-focus-promo-title",
            },
            subtitle: {
              string_id: "spotlight-focus-promo-subtitle",
            },
            dismiss_button: {
              action: {
                dismiss: true,
              },
            },
            ios: {
              action: {
                data: {
                  args: "https://app.adjust.com/167k4ih?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fapps.apple.com%2Fus%2Fapp%2Ffirefox-focus-privacy-browser%2Fid1055677337",
                  where: "tabshifted",
                },
                type: "OPEN_URL",
                navigate: true,
              },
            },
            android: {
              action: {
                data: {
                  args: "https://app.adjust.com/167k4ih?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dorg.mozilla.focus",
                  where: "tabshifted",
                },
                type: "OPEN_URL",
                navigate: true,
              },
            },
            email_link: {
              action: {
                data: {
                  args: "https://mozilla.org",
                  where: "tabshifted",
                },
                type: "OPEN_URL",
                navigate: true,
              },
            },
            tiles: {
              type: "mobile_downloads",
              data: {
                QR_code: {
                  image_url:
                    "chrome://browser/content/assets/focus-qr-code.svg",
                  alt_text: {
                    string_id: "spotlight-focus-promo-qr-code",
                  },
                },
                email: {
                  link_text: "Email yourself a link",
                },
                marketplace_buttons: ["ios", "android"],
              },
            },
          },
        },
      ],
    },
    trigger: { id: "defaultBrowserCheck" },
  },
  {
    id: "PB_NEWTAB_VPN_PROMO",
    template: "pb_newtab",
    content: {
      promoEnabled: true,
      promoType: "VPN",
      infoEnabled: true,
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
      promoButton: {
        action: {
          type: "OPEN_URL",
          data: {
            args: "https://vpn.mozilla.org/",
          },
        },
      },
    },
    groups: ["panel-test-provider"],
    targeting: "region != 'CN' && !hasActiveEnterprisePolicies",
    frequency: { lifetime: 3 },
  },
  {
    id: "PB_PIN_PROMO",
    template: "pb_newtab",
    groups: ["pbNewtab"],
    content: {
      infoBody: "fluent:about-private-browsing-info-description-simplified",
      infoEnabled: true,
      infoIcon: "chrome://global/skin/icons/indicator-private-browsing.svg",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitle: "",
      infoTitleEnabled: false,
      promoEnabled: true,
      promoType: "PIN",
      promoHeader: "Private browsing freedom in one click",
      promoImageLarge:
        "chrome://browser/content/assets/private-promo-asset.svg",
      promoLinkText: "Pin To Taskbar",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle:
        "No saved cookies or history, right from your desktop. Browse like no one’s watching.",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "MULTI_ACTION",
          data: {
            actions: [
              {
                type: "SET_PREF",
                data: {
                  pref: {
                    name: "browser.privateWindowSeparation.enabled",
                    value: true,
                  },
                },
              },
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
              {
                type: "BLOCK_MESSAGE",
                data: {
                  id: "PB_PIN_PROMO",
                },
              },
              {
                type: "OPEN_ABOUT_PAGE",
                data: { args: "privatebrowsing", where: "current" },
              },
            ],
          },
        },
      },
    },
    priority: 3,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting:
      "region != 'CN' && !hasActiveEnterprisePolicies && doesAppNeedPin",
  },
  {
    id: "TEST_TOAST_NOTIFICATION1",
    weight: 100,
    template: "toast_notification",
    content: {
      title: {
        string_id: "cfr-doorhanger-bookmark-fxa-header",
      },
      body: "Body",
      image_url:
        "https://firefox-settings-attachments.cdn.mozilla.net/main-workspace/ms-images/a3c640c8-7594-4bb2-bc18-8b4744f3aaf2.gif",
      launch_url: "https://mozilla.org",
      requireInteraction: true,
      actions: [
        {
          action: "dismiss",
          title: "Dismiss",
          windowsSystemActivationType: true,
        },
        {
          action: "snooze",
          title: "Snooze",
          windowsSystemActivationType: true,
        },
        { action: "callback", title: "Callback" },
      ],
      tag: "test_toast_notification",
    },
    groups: ["panel-test-provider"],
    targeting: "!hasActiveEnterprisePolicies",
    trigger: { id: "backgroundTaskMessage" },
    frequency: { lifetime: 3 },
  },
  {
    id: "TEST_TOAST_NOTIFICATION2",
    weight: 100,
    template: "toast_notification",
    content: {
      title: "Launch action on toast click and on action button click",
      body: "Body",
      image_url:
        "https://firefox-settings-attachments.cdn.mozilla.net/main-workspace/ms-images/a3c640c8-7594-4bb2-bc18-8b4744f3aaf2.gif",
      launch_action: {
        type: "OPEN_URL",
        data: { args: "https://mozilla.org", where: "window" },
      },
      requireInteraction: true,
      actions: [
        {
          action: "dismiss",
          title: "Dismiss",
          windowsSystemActivationType: true,
        },
        {
          action: "snooze",
          title: "Snooze",
          windowsSystemActivationType: true,
        },
        {
          action: "private",
          title: "Private Window",
          launch_action: { type: "OPEN_PRIVATE_BROWSER_WINDOW" },
        },
      ],
      tag: "test_toast_notification",
    },
    groups: ["panel-test-provider"],
    targeting: "!hasActiveEnterprisePolicies",
    trigger: { id: "backgroundTaskMessage" },
    frequency: { lifetime: 3 },
  },
  {
    id: "MR2022_BACKGROUND_UPDATE_TOAST_NOTIFICATION",
    weight: 100,
    template: "toast_notification",
    content: {
      title: {
        string_id: "mr2022-background-update-toast-title",
      },
      body: {
        string_id: "mr2022-background-update-toast-text",
      },
      image_url:
        "https://firefox-settings-attachments.cdn.mozilla.net/main-workspace/ms-images/673d2808-e5d8-41b9-957e-f60d53233b97.png",
      requireInteraction: true,
      actions: [
        {
          action: "open",
          title: {
            string_id: "mr2022-background-update-toast-primary-button-label",
          },
        },
        {
          action: "snooze",
          windowsSystemActivationType: true,
          title: {
            string_id: "mr2022-background-update-toast-secondary-button-label",
          },
        },
      ],
      tag: "mr2022_background_update",
    },
    groups: ["panel-test-provider"],
    targeting: "!hasActiveEnterprisePolicies",
    trigger: { id: "backgroundTaskMessage" },
    frequency: { lifetime: 3 },
  },
  {
    id: "IMPORT_SETTINGS_EMBEDDED",
    groups: ["panel-test-provider"],
    template: "spotlight",
    content: {
      template: "multistage",
      backdrop: "transparent",
      screens: [
        {
          id: "IMPORT_SETTINGS_EMBEDDED",
          content: {
            logo: {},
            tiles: { type: "migration-wizard" },
            progress_bar: true,
            migrate_start: {
              action: {},
            },
            migrate_close: {
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
              has_arrow_icon: true,
            },
          },
        },
      ],
    },
  },
  {
    id: "TEST_FEATURE_TOUR",
    template: "feature_callout",
    groups: [],
    content: {
      id: "TEST_FEATURE_TOUR",
      template: "multistage",
      backdrop: "transparent",
      transitions: false,
      disableHistoryUpdates: true,
      screens: [
        {
          id: "FEATURE_CALLOUT_1",
          anchors: [
            {
              selector: "#PanelUI-menu-button",
              panel_position: {
                anchor_attachment: "bottomcenter",
                callout_attachment: "topright",
              },
            },
          ],
          content: {
            position: "callout",
            title: { raw: "Panel Feature Callout" },
            subtitle: { raw: "Hello!" },
            secondary_button: {
              label: { raw: "Advance" },
              action: { navigate: true },
            },
            submenu_button: {
              submenu: [
                {
                  type: "action",
                  label: { raw: "Item 1" },
                  action: { navigate: true },
                  id: "item1",
                },
                {
                  type: "action",
                  label: { raw: "Item 2" },
                  action: { navigate: true },
                  id: "item2",
                },
                {
                  type: "menu",
                  label: { raw: "Menu 1" },
                  submenu: [
                    {
                      type: "action",
                      label: { raw: "Item 3" },
                      action: { navigate: true },
                      id: "item3",
                    },
                    {
                      type: "action",
                      label: { raw: "Item 4" },
                      action: { navigate: true },
                      id: "item4",
                    },
                  ],
                  id: "menu1",
                },
              ],
              attached_to: "secondary_button",
            },
            dismiss_button: {
              action: { dismiss: true },
            },
          },
        },
      ],
    },
  },
];

export const PanelTestProvider = {
  getMessages() {
    return Promise.resolve(
      MESSAGES().map(message => ({
        ...message,
        targeting: `providerCohorts.panel_local_testing == "SHOW_TEST"`,
      }))
    );
  },
};
