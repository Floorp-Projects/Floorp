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
      body:
        "Contribute your data to Mozilla's Pioneer program to help researchers understand pressing technology issues like misinformation, data privacy, and ethical AI.",
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
      logo: {
        imageURL: "chrome://browser/content/logos/vpn-promo-logo.svg",
      },
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
    id: "TCP_CFR_MESSAGE_103",
    groups: ["cfr"],
    template: "cfr_doorhanger",
    content: {
      bucket_id: "TCP_CFR",
      layout: "icon_and_message",
      icon: "chrome://branding/content/about-logo@2x.png",
      icon_class: "cfr-doorhanger-large-icon",
      heading_text: {
        string_id: "cfr-total-cookie-protection-header",
      },
      text: {
        string_id: "cfr-total-cookie-protection-body",
      },
      buttons: {
        primary: {
          label: {
            string_id: "cfr-doorhanger-milestone-close-button",
          },
          action: {
            type: "CANCEL",
          },
        },
        secondary: [],
      },
      anchor_id: "tracking-protection-icon-container",
      skip_address_bar_notifier: true,
    },
    frequency: { lifetime: 1 },
    trigger: {
      id: "openURL",
      patterns: ["*://*/*"],
    },
    targeting:
      "firefoxVersion >= 103 && 'privacy.restrict3rdpartystorage.rollout.enabledByDefault'|preferenceValue",
  },
  {
    id: "BETTER_INTERNET_GLOBAL_ROLLOUT",
    groups: ["eco"],
    content: {
      template: "logo-and-content",
      logo: {
        imageURL:
          "chrome://activity-stream/content/data/content/assets/remote/mountain.svg",
        size: "115px",
      },
      body: {
        title: {
          label: {
            string_id: "spotlight-better-internet-header",
          },
          size: "22px",
        },
        text: {
          label: {
            string_id: "spotlight-better-internet-body",
          },
          size: "16px",
        },
        primary: {
          label: {
            string_id: "spotlight-pin-primary-button",
          },
          action: {
            type: "PIN_FIREFOX_TO_TASKBAR",
          },
        },
        secondary: {
          label: {
            string_id: "spotlight-pin-secondary-button",
          },
          action: {
            type: "CANCEL",
          },
        },
      },
    },
    trigger: {
      id: "defaultBrowserCheck",
    },
    template: "spotlight",
    frequency: {
      lifetime: 1,
    },
    targeting:
      "userMonthlyActivity|length >= 1 && userMonthlyActivity|length <= 6 && doesAppNeedPin",
  },
  {
    id: "PEACE_OF_MIND_GLOBAL_ROLLOUT",
    groups: ["eco"],
    content: {
      template: "logo-and-content",
      logo: {
        imageURL:
          "chrome://activity-stream/content/data/content/assets/remote/umbrella.png",
        size: "115px",
      },
      body: {
        title: {
          label: {
            string_id: "spotlight-peace-mind-header",
          },
          size: "22px",
        },
        text: {
          label: {
            string_id: "spotlight-peace-mind-body",
          },
          size: "15px",
        },
        primary: {
          label: {
            string_id: "spotlight-pin-primary-button",
          },
          action: {
            type: "PIN_FIREFOX_TO_TASKBAR",
          },
        },
        secondary: {
          label: {
            string_id: "spotlight-pin-secondary-button",
          },
          action: {
            type: "CANCEL",
          },
        },
      },
    },
    trigger: {
      id: "defaultBrowserCheck",
    },
    template: "spotlight",
    frequency: {
      lifetime: 1,
    },
    targeting:
      "userMonthlyActivity|length >= 7 && userMonthlyActivity|length <= 13 && doesAppNeedPin",
  },
  {
    id: "MULTISTAGE_SPOTLIGHT_MESSAGE",
    groups: ["panel-test-provider"],
    template: "spotlight",
    content: {
      id: "control",
      template: "multistage",
      backdrop: "transparent",
      transitions: true,
      screens: [
        {
          id: "AW_PIN_FIREFOX",
          content: {
            has_noodles: true,
            title: {
              string_id: "mr1-onboarding-pin-header",
            },
            logo: {},
            hero_text: {
              string_id: "mr1-welcome-screen-hero-text",
            },
            help_text: {
              text: {
                string_id: "mr1-onboarding-welcome-image-caption",
              },
            },
            primary_button: {
              label: {
                string_id: "mr1-onboarding-pin-primary-button-label",
              },
              action: {
                navigate: true,
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
            },
            secondary_button: {
              label: {
                string_id: "mr1-onboarding-set-default-secondary-button-label",
              },
              action: {
                navigate: true,
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
              string_id: "mr1-onboarding-default-header",
            },
            subtitle: {
              string_id: "mr1-onboarding-default-subtitle",
            },
            primary_button: {
              label: {
                string_id: "mr1-onboarding-default-primary-button-label",
              },
              action: {
                navigate: true,
                type: "SET_DEFAULT_BROWSER",
              },
            },
            secondary_button: {
              label: {
                string_id: "mr1-onboarding-set-default-secondary-button-label",
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
            background:
              "url(chrome://activity-stream/content/data/content/assets/proton-bkg.avif) no-repeat center/cover",
            text_color: "light",
            logo: {
              imageURL:
                "https://firefox-settings-attachments.cdn.mozilla.net/main-workspace/ms-images/a3c640c8-7594-4bb2-bc18-8b4744f3aaf2.gif",
            },
            title: "A dialog with a background image",
            subtitle: "The text color is configurable",
            primary_button: {
              label: "Continue",
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr1-onboarding-set-default-secondary-button-label",
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
            subtitle:
              "For the best privacy protection, keep Firefox in easy reach.",
            primary_button: {
              label: "Continue",
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr1-onboarding-set-default-secondary-button-label",
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
                navigate: true,
              },
            },
            ios: {
              action: {
                data: {
                  args:
                    "https://app.adjust.com/167k4ih?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fapps.apple.com%2Fus%2Fapp%2Ffirefox-focus-privacy-browser%2Fid1055677337",
                  where: "tabshifted",
                },
                type: "OPEN_URL",
                navigate: true,
              },
            },
            android: {
              action: {
                data: {
                  args:
                    "https://app.adjust.com/167k4ih?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dorg.mozilla.focus",
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
                  image_overrides: {
                    de: "chrome://browser/content/assets/klar-qr-code.svg",
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

const EXPORTED_SYMBOLS = ["PanelTestProvider"];
