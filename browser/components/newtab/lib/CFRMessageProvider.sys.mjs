/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const FACEBOOK_CONTAINER_PARAMS = {
  existing_addons: [
    "@contain-facebook",
    "{bb1b80be-e6b3-40a1-9b6e-9d4073343f0b}",
    "{a50d61ca-d27b-437a-8b52-5fd801a0a88b}",
  ],
  open_urls: ["www.facebook.com", "facebook.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const GOOGLE_TRANSLATE_PARAMS = {
  existing_addons: [
    "jid1-93WyvpgvxzGATw@jetpack",
    "{087ef4e1-4286-4be6-9aa3-8d6c420ee1db}",
    "{4170faaa-ee87-4a0e-b57a-1aec49282887}",
    "jid1-TMndP6cdKgxLcQ@jetpack",
    "s3google@translator",
    "{9c63d15c-b4d9-43bd-b223-37f0a1f22e2a}",
    "translator@zoli.bod",
    "{8cda9ce6-7893-4f47-ac70-a65215cec288}",
    "simple-translate@sienori",
    "@translatenow",
    "{a79fafce-8da6-4685-923f-7ba1015b8748})",
    "{8a802b5a-eeab-11e2-a41d-b0096288709b}",
    "jid0-fbHwsGfb6kJyq2hj65KnbGte3yT@jetpack",
    "storetranslate.plugin@gmail.com",
    "jid1-r2tWDbSkq8AZK1@jetpack",
    "{b384b75c-c978-4c4d-b3cf-62a82d8f8f12}",
    "jid1-f7dnBeTj8ElpWQ@jetpack",
    "{dac8a935-4775-4918-9205-5c0600087dc4}",
    "gtranslation2@slam.com",
    "{e20e0de5-1667-4df4-bd69-705720e37391}",
    "{09e26ae9-e9c1-477c-80a6-99934212f2fe}",
    "mgxtranslator@magemagix.com",
    "gtranslatewins@mozilla.org",
  ],
  open_urls: ["translate.google.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const YOUTUBE_ENHANCE_PARAMS = {
  existing_addons: [
    "enhancerforyoutube@maximerf.addons.mozilla.org",
    "{dc8f61ab-5e98-4027-98ef-bb2ff6060d71}",
    "{7b1bf0b6-a1b9-42b0-b75d-252036438bdc}",
    "jid0-UVAeBCfd34Kk5usS8A1CBiobvM8@jetpack",
    "iridium@particlecore.github.io",
    "jid1-ss6kLNCbNz6u0g@jetpack",
    "{1cf918d2-f4ea-4b4f-b34e-455283fef19f}",
  ],
  open_urls: ["www.youtube.com", "youtube.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS = {
  existing_addons: [
    "@wikipediacontextmenusearch",
    "{ebf47fc8-01d8-4dba-aa04-2118402f4b20}",
    "{5737a280-b359-4e26-95b0-adec5915a854}",
    "olivier.debroqueville@gmail.com",
    "{3923146e-98cb-472b-9c13-f6849d34d6b8}",
  ],
  open_urls: ["www.wikipedia.org", "wikipedia.org"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};
const REDDIT_ENHANCEMENT_PARAMS = {
  existing_addons: ["jid1-xUfzOsOFlzSOXg@jetpack"],
  open_urls: ["www.reddit.com", "reddit.com"],
  sumo_path: "extensionrecommendations",
  min_frecency: 10000,
};

const CFR_MESSAGES = [
  {
    id: "FACEBOOK_CONTAINER_3",
    template: "cfr_doorhanger",
    groups: ["cfr-message-provider"],
    content: {
      layout: "addon_recommendation",
      category: "cfrAddons",
      bucket_id: "CFR_M1",
      notification_text: {
        string_id: "cfr-doorhanger-extension-notification2",
      },
      heading_text: { string_id: "cfr-doorhanger-extension-heading" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: FACEBOOK_CONTAINER_PARAMS.sumo_path,
      },
      addon: {
        id: "954390",
        title: "Facebook Container",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        rating: "4.6",
        users: "299019",
        author: "Mozilla",
        amo_url: "https://addons.mozilla.org/firefox/addon/facebook-container/",
      },
      text: "Stop Facebook from tracking your activity across the web. Use Facebook the way you normally do without annoying ads following you around.",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-extension-ok-button" },
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: { url: "https://example.com", telemetrySource: "amo" },
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
              data: { category: "general-cfraddons" },
            },
          },
        ],
      },
    },
    frequency: { lifetime: 3 },
    targeting: `
      localeLanguageCode == "en" &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(
        FACEBOOK_CONTAINER_PARAMS.existing_addons
      )} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(
        FACEBOOK_CONTAINER_PARAMS.open_urls
      )} intersect topFrecentSites[.frecency >= ${
      FACEBOOK_CONTAINER_PARAMS.min_frecency
    }]|mapToProperty('host'))|length > 0`,
    trigger: { id: "openURL", params: FACEBOOK_CONTAINER_PARAMS.open_urls },
  },
  {
    id: "GOOGLE_TRANSLATE_3",
    groups: ["cfr-message-provider"],
    template: "cfr_doorhanger",
    content: {
      layout: "addon_recommendation",
      category: "cfrAddons",
      bucket_id: "CFR_M1",
      notification_text: {
        string_id: "cfr-doorhanger-extension-notification2",
      },
      heading_text: { string_id: "cfr-doorhanger-extension-heading" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: GOOGLE_TRANSLATE_PARAMS.sumo_path,
      },
      addon: {
        id: "445852",
        title: "To Google Translate",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        rating: "4.1",
        users: "313474",
        author: "Juan Escobar",
        amo_url:
          "https://addons.mozilla.org/firefox/addon/to-google-translate/",
      },
      text: "Instantly translate any webpage text. Simply highlight the text, right-click to open the context menu, and choose a text or aural translation.",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-extension-ok-button" },
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: { url: "https://example.com", telemetrySource: "amo" },
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
              data: { category: "general-cfraddons" },
            },
          },
        ],
      },
    },
    frequency: { lifetime: 3 },
    targeting: `
      localeLanguageCode == "en" &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(
        GOOGLE_TRANSLATE_PARAMS.existing_addons
      )} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(
        GOOGLE_TRANSLATE_PARAMS.open_urls
      )} intersect topFrecentSites[.frecency >= ${
      GOOGLE_TRANSLATE_PARAMS.min_frecency
    }]|mapToProperty('host'))|length > 0`,
    trigger: { id: "openURL", params: GOOGLE_TRANSLATE_PARAMS.open_urls },
  },
  {
    id: "YOUTUBE_ENHANCE_3",
    groups: ["cfr-message-provider"],
    template: "cfr_doorhanger",
    content: {
      layout: "addon_recommendation",
      category: "cfrAddons",
      bucket_id: "CFR_M1",
      notification_text: {
        string_id: "cfr-doorhanger-extension-notification2",
      },
      heading_text: { string_id: "cfr-doorhanger-extension-heading" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: YOUTUBE_ENHANCE_PARAMS.sumo_path,
      },
      addon: {
        id: "700308",
        title: "Enhancer for YouTube\u2122",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        rating: "4.8",
        users: "357328",
        author: "Maxime RF",
        amo_url:
          "https://addons.mozilla.org/firefox/addon/enhancer-for-youtube/",
      },
      text: "Take control of your YouTube experience. Automatically block annoying ads, set playback speed and volume, remove annotations, and more.",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-extension-ok-button" },
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: { url: "https://example.com", telemetrySource: "amo" },
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
              data: { category: "general-cfraddons" },
            },
          },
        ],
      },
    },
    frequency: { lifetime: 3 },
    targeting: `
      localeLanguageCode == "en" &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(
        YOUTUBE_ENHANCE_PARAMS.existing_addons
      )} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(
        YOUTUBE_ENHANCE_PARAMS.open_urls
      )} intersect topFrecentSites[.frecency >= ${
      YOUTUBE_ENHANCE_PARAMS.min_frecency
    }]|mapToProperty('host'))|length > 0`,
    trigger: { id: "openURL", params: YOUTUBE_ENHANCE_PARAMS.open_urls },
  },
  {
    id: "WIKIPEDIA_CONTEXT_MENU_SEARCH_3",
    groups: ["cfr-message-provider"],
    template: "cfr_doorhanger",
    exclude: true,
    content: {
      layout: "addon_recommendation",
      category: "cfrAddons",
      bucket_id: "CFR_M1",
      notification_text: {
        string_id: "cfr-doorhanger-extension-notification2",
      },
      heading_text: { string_id: "cfr-doorhanger-extension-heading" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.sumo_path,
      },
      addon: {
        id: "659026",
        title: "Wikipedia Context Menu Search",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        rating: "4.9",
        users: "3095",
        author: "Nick Diedrich",
        amo_url:
          "https://addons.mozilla.org/firefox/addon/wikipedia-context-menu-search/",
      },
      text: "Get to a Wikipedia page fast, from anywhere on the web. Just highlight any webpage text and right-click to open the context menu to start a Wikipedia search.",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-extension-ok-button" },
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: { url: "https://example.com", telemetrySource: "amo" },
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
              data: { category: "general-cfraddons" },
            },
          },
        ],
      },
    },
    frequency: { lifetime: 3 },
    targeting: `
      localeLanguageCode == "en" &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(
        WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.existing_addons
      )} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(
        WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.open_urls
      )} intersect topFrecentSites[.frecency >= ${
      WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.min_frecency
    }]|mapToProperty('host'))|length > 0`,
    trigger: {
      id: "openURL",
      params: WIKIPEDIA_CONTEXT_MENU_SEARCH_PARAMS.open_urls,
    },
  },
  {
    id: "REDDIT_ENHANCEMENT_3",
    groups: ["cfr-message-provider"],
    template: "cfr_doorhanger",
    exclude: true,
    content: {
      layout: "addon_recommendation",
      category: "cfrAddons",
      bucket_id: "CFR_M1",
      notification_text: {
        string_id: "cfr-doorhanger-extension-notification2",
      },
      heading_text: { string_id: "cfr-doorhanger-extension-heading" },
      info_icon: {
        label: { string_id: "cfr-doorhanger-extension-sumo-link" },
        sumo_path: REDDIT_ENHANCEMENT_PARAMS.sumo_path,
      },
      addon: {
        id: "387429",
        title: "Reddit Enhancement Suite",
        icon: "chrome://browser/skin/addons/addon-install-downloading.svg",
        rating: "4.6",
        users: "258129",
        author: "honestbleeps",
        amo_url:
          "https://addons.mozilla.org/firefox/addon/reddit-enhancement-suite/",
      },
      text: "New features include Inline Image Viewer, Never Ending Reddit (never click 'next page' again), Keyboard Navigation, Account Switcher, and User Tagger.",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-extension-ok-button" },
          action: {
            type: "INSTALL_ADDON_FROM_URL",
            data: { url: "https://example.com", telemetrySource: "amo" },
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
              data: { category: "general-cfraddons" },
            },
          },
        ],
      },
    },
    frequency: { lifetime: 3 },
    targeting: `
      localeLanguageCode == "en" &&
      (xpinstallEnabled == true) &&
      (${JSON.stringify(
        REDDIT_ENHANCEMENT_PARAMS.existing_addons
      )} intersect addonsInfo.addons|keys)|length == 0 &&
      (${JSON.stringify(
        REDDIT_ENHANCEMENT_PARAMS.open_urls
      )} intersect topFrecentSites[.frecency >= ${
      REDDIT_ENHANCEMENT_PARAMS.min_frecency
    }]|mapToProperty('host'))|length > 0`,
    trigger: { id: "openURL", params: REDDIT_ENHANCEMENT_PARAMS.open_urls },
  },
  {
    id: "DOH_ROLLOUT_CONFIRMATION",
    groups: ["cfr-message-provider"],
    targeting: `
      "doh-rollout.enabled"|preferenceValue &&
      !"doh-rollout.disable-heuristics"|preferenceValue &&
      !"doh-rollout.skipHeuristicsCheck"|preferenceValue &&
      !"doh-rollout.doorhanger-decision"|preferenceValue
    `,
    template: "cfr_doorhanger",
    content: {
      skip_address_bar_notifier: true,
      persistent_doorhanger: true,
      anchor_id: "PanelUI-menu-button",
      layout: "icon_and_message",
      text: { string_id: "cfr-doorhanger-doh-body" },
      icon: "chrome://global/skin/icons/security.svg",
      buttons: {
        secondary: [
          {
            label: { string_id: "cfr-doorhanger-doh-secondary-button" },
            action: {
              type: "DISABLE_DOH",
            },
          },
        ],
        primary: {
          label: { string_id: "cfr-doorhanger-doh-primary-button-2" },
          action: {
            type: "ACCEPT_DOH",
          },
        },
      },
      bucket_id: "DOH_ROLLOUT_CONFIRMATION",
      heading_text: { string_id: "cfr-doorhanger-doh-header" },
      info_icon: {
        label: {
          string_id: "cfr-doorhanger-extension-sumo-link",
        },
        sumo_path: "extensionrecommendations",
      },
      notification_text: "Message from Firefox",
      category: "cfrFeatures",
    },
    trigger: {
      id: "openURL",
      patterns: ["*://*/*"],
    },
  },
  {
    id: "SAVE_LOGIN",
    groups: ["cfr-message-provider"],
    frequency: {
      lifetime: 3,
    },
    targeting:
      "(!type || type == 'save') && isFxAEnabled == true && usesFirefoxSync == false",
    template: "cfr_doorhanger",
    content: {
      layout: "icon_and_message",
      text: "Securely store and sync your passwords to all your devices.",
      icon: "chrome://browser/content/aboutlogins/icons/intro-illustration.svg",
      icon_class: "cfr-doorhanger-large-icon",
      buttons: {
        secondary: [
          {
            label: {
              string_id: "cfr-doorhanger-extension-cancel-button",
            },
            action: {
              type: "CANCEL",
            },
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
              data: {
                category: "general-cfrfeatures",
              },
            },
          },
        ],
        primary: {
          label: {
            value: "Turn on Sync",
            attributes: { accesskey: "T" },
          },
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {
              category: "sync",
              entrypoint: "cfr-save-login",
            },
          },
        },
      },
      bucket_id: "CFR_SAVE_LOGIN",
      heading_text: "Never Lose a Password Again",
      info_icon: {
        label: {
          string_id: "cfr-doorhanger-extension-sumo-link",
        },
        sumo_path: "extensionrecommendations",
      },
      notification_text: {
        string_id: "cfr-doorhanger-feature-notification",
      },
      category: "cfrFeatures",
    },
    trigger: {
      id: "newSavedLogin",
    },
  },
  {
    id: "UPDATE_LOGIN",
    groups: ["cfr-message-provider"],
    frequency: {
      lifetime: 3,
    },
    targeting:
      "type == 'update' && isFxAEnabled == true && usesFirefoxSync == false",
    template: "cfr_doorhanger",
    content: {
      layout: "icon_and_message",
      text: "Securely store and sync your passwords to all your devices.",
      icon: "chrome://browser/content/aboutlogins/icons/intro-illustration.svg",
      icon_class: "cfr-doorhanger-large-icon",
      buttons: {
        secondary: [
          {
            label: {
              string_id: "cfr-doorhanger-extension-cancel-button",
            },
            action: {
              type: "CANCEL",
            },
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
              data: {
                category: "general-cfrfeatures",
              },
            },
          },
        ],
        primary: {
          label: {
            value: "Turn on Sync",
            attributes: { accesskey: "T" },
          },
          action: {
            type: "OPEN_PREFERENCES_PAGE",
            data: {
              category: "sync",
              entrypoint: "cfr-update-login",
            },
          },
        },
      },
      bucket_id: "CFR_UPDATE_LOGIN",
      heading_text: "Never Lose a Password Again",
      info_icon: {
        label: {
          string_id: "cfr-doorhanger-extension-sumo-link",
        },
        sumo_path: "extensionrecommendations",
      },
      notification_text: {
        string_id: "cfr-doorhanger-feature-notification",
      },
      category: "cfrFeatures",
    },
    trigger: {
      id: "newSavedLogin",
    },
  },
  {
    id: "MILESTONE_MESSAGE",
    groups: ["cfr-message-provider"],
    template: "milestone_message",
    content: {
      layout: "short_message",
      category: "cfrFeatures",
      anchor_id: "tracking-protection-icon-container",
      skip_address_bar_notifier: true,
      bucket_id: "CFR_MILESTONE_MESSAGE",
      heading_text: { string_id: "cfr-doorhanger-milestone-heading2" },
      notification_text: "",
      text: "",
      buttons: {
        primary: {
          label: { string_id: "cfr-doorhanger-milestone-ok-button" },
          action: { type: "OPEN_PROTECTION_REPORT" },
          event: "PROTECTION",
        },
        secondary: [
          {
            label: { string_id: "cfr-doorhanger-milestone-close-button" },
            action: { type: "CANCEL" },
            event: "DISMISS",
          },
        ],
      },
    },
    targeting: "pageLoad >= 1",
    frequency: {
      lifetime: 7, // Length of privacy.contentBlocking.cfr-milestone.milestones pref
    },
    trigger: {
      id: "contentBlocking",
      params: ["ContentBlockingMilestone"],
    },
  },
  {
    id: "HEARTBEAT_TACTIC_2",
    groups: ["cfr-message-provider"],
    template: "cfr_urlbar_chiclet",
    content: {
      layout: "chiclet_open_url",
      category: "cfrHeartbeat",
      bucket_id: "HEARTBEAT_TACTIC_2",
      notification_text: "Improve Firefox",
      active_color: "#595e91",
      action: {
        url: "http://example.com/%VERSION%/",
        where: "tabshifted",
      },
    },
    targeting: "false",
    frequency: {
      lifetime: 3,
    },
    trigger: {
      id: "openURL",
      patterns: ["*://*/*"],
    },
  },
  {
    id: "HOMEPAGE_REMEDIATION_82",
    groups: ["cfr-message-provider"],
    frequency: {
      lifetime: 3,
    },
    targeting:
      "!homePageSettings.isDefault && homePageSettings.isCustomUrl && homePageSettings.urls[.host == 'google.com']|length > 0 && visitsCount >= 3 && userPrefs.cfrFeatures",
    template: "cfr_doorhanger",
    content: {
      layout: "icon_and_message",
      text: "Update your homepage to search Google while also being able to search your Firefox history and bookmarks.",
      icon: "chrome://global/skin/icons/search-glass.svg",
      buttons: {
        secondary: [
          {
            label: {
              string_id: "cfr-doorhanger-extension-cancel-button",
            },
            action: {
              type: "CANCEL",
            },
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
              data: {
                category: "general-cfrfeatures",
              },
            },
          },
        ],
        primary: {
          label: {
            value: "Activate now",
            attributes: {
              accesskey: "A",
            },
          },
          action: {
            type: "CONFIGURE_HOMEPAGE",
            data: {
              homePage: "default",
              newtab: "default",
              layout: {
                search: true,
                topsites: false,
                highlights: false,
                topstories: false,
              },
            },
          },
        },
      },
      bucket_id: "HOMEPAGE_REMEDIATION_82",
      heading_text: "A better search experience",
      info_icon: {
        label: {
          string_id: "cfr-doorhanger-extension-sumo-link",
        },
        sumo_path: "extensionrecommendations",
      },
      notification_text: {
        string_id: "cfr-doorhanger-feature-notification",
      },
      category: "cfrFeatures",
    },
    trigger: {
      id: "openURL",
      params: ["google.com", "www.google.com"],
    },
  },
  {
    id: "INFOBAR_ACTION_86",
    groups: ["cfr-message-provider"],
    targeting: "false",
    template: "infobar",
    content: {
      type: "global",
      text: { string_id: "default-browser-notification-message" },
      buttons: [
        {
          label: { string_id: "default-browser-notification-button" },
          primary: true,
          accessKey: "O",
          action: {
            type: "SET_DEFAULT_BROWSER",
          },
        },
      ],
    },
    trigger: { id: "defaultBrowserCheck" },
  },
  {
    id: "PREF_OBSERVER_MESSAGE_94",
    groups: ["cfr-message-provider"],
    targeting: "true",
    template: "infobar",
    content: {
      type: "global",
      text: "This is a message triggered when a pref value changes",
      buttons: [
        {
          label: "OK",
          primary: true,
          accessKey: "O",
          action: {
            type: "CANCEL",
          },
        },
      ],
    },
    trigger: { id: "preferenceObserver", params: ["foo.bar"] },
  },
];

export const CFRMessageProvider = {
  getMessages() {
    return Promise.resolve(CFR_MESSAGES.filter(msg => !msg.exclude));
  },
};
