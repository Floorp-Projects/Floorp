/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* globals Localization */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  ShellService: "resource:///modules/ShellService.jsm",
  BuiltInThemes: "resource:///modules/BuiltInThemes.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "usesFirefoxSync",
  "services.sync.username"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "mobileDevices",
  "services.sync.clients.devices.mobile",
  0
);

const L10N = new Localization([
  "branding/brand.ftl",
  "browser/branding/brandings.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
]);

const ONBOARDING_MESSAGES = () => [
  {
    id: "FXA_ACCOUNTS_BADGE",
    template: "toolbar_badge",
    content: {
      delay: 10000, // delay for 10 seconds
      target: "fxa-toolbar-menu-button",
    },
    targeting: "false",
    trigger: { id: "toolbarBadgeUpdate" },
  },
  {
    id: "PROTECTIONS_PANEL_1",
    template: "protections_panel",
    content: {
      title: { string_id: "cfr-protections-panel-header" },
      body: { string_id: "cfr-protections-panel-body" },
      link_text: { string_id: "cfr-protections-panel-link-text" },
      cta_url: `${Services.urlFormatter.formatURLPref(
        "app.support.baseURL"
      )}etp-promotions?as=u&utm_source=inproduct`,
      cta_type: "OPEN_URL",
    },
    trigger: { id: "protectionsPanelOpen" },
  },
  {
    id: "FX_MR_106_UPGRADE",
    template: "spotlight",
    targeting: "true",
    content: {
      template: "multistage",
      id: "FX_MR_106_UPGRADE",
      transitions: true,
      screens: [
        {
          id: "UPGRADE_PIN_FIREFOX",
          content: {
            position: "split",
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-pintaskbar.svg') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(83.12% 83.12% at 80.59% 16.88%, rgba(103, 51, 205, 0.75) 0%, rgba(0, 108, 207, 0.75) 54.51%, rgba(128, 199, 247, 0.75) 100%)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-existing-pin-header",
            },
            subtitle: {
              string_id: "mr2022-onboarding-existing-pin-subtitle",
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
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
        {
          id: "UPGRADE_SET_DEFAULT",
          content: {
            position: "split",
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(113% 87.18% at 93.5% 73.82%, rgba(103, 51, 205, 0.75) 0%, rgba(0, 108, 207, 0.75) 54.51%, rgba(128, 199, 247, 0.75) 100%)",
            logo: {},
            title: {
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
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
        {
          id: "UPGRADE_IMPORT_SETTINGS",
          content: {
            position: "split",
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-import.svg') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(120.14% 108.82% at 69.5% 100%, rgba(103, 51, 205, 0.75) 0%, rgba(0, 108, 207, 0.75) 54.51%, rgba(128, 199, 247, 0.75) 100%)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-import-header",
            },
            subtitle: {
              string_id: "mr2022-onboarding-import-subtitle",
            },
            primary_button: {
              label: {
                string_id:
                  "mr2022-onboarding-import-primary-button-label-no-attribution",
              },
              action: {
                type: "SHOW_MIGRATION_WIZARD",
                data: {},
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
            },
          },
        },
        {
          id: "UPGRADE_COLORWAY",
          content: {
            position: "split",
            split_narrow_bkg_position: "-100px",
            background:
              "url('chrome://browser/content/colorways/assets/independent-voices-collection.avif') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(83.12% 83.12% at 80.59% 16.88%, #9059FF 0%, #3A8EE6 54.51%, #A0C4EA 100%)",
            progress_bar: true,
            logo: {},
            title: {
              string_id: "mr2022-onboarding-colorway-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-colorway-subtitle",
            },
            tiles: {
              type: "colorway",
              action: {
                theme: "<event>",
              },
              defaultVariationIndex: 1,
              systemVariations: ["light", "automatic", "dark"],
              variations: ["soft", "balanced", "bold"],
              colorways: [
                {
                  id: "default",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-default",
                  },
                  tooltip: {
                    string_id: "mr2022-onboarding-colorway-tooltip-default",
                  },
                  description: {
                    string_id: "mr2022-onboarding-colorway-description-default",
                  },
                },
                {
                  id: "playmaker",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-playmaker",
                  },
                  tooltip: {
                    string_id: "mr2022-onboarding-colorway-tooltip-playmaker",
                  },
                  description: {
                    string_id:
                      "mr2022-onboarding-colorway-description-playmaker",
                  },
                },
                {
                  id: "expressionist",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-expressionist",
                  },
                  tooltip: {
                    string_id:
                      "mr2022-onboarding-colorway-tooltip-expressionist",
                  },
                  description: {
                    string_id:
                      "mr2022-onboarding-colorway-description-expressionist",
                  },
                },
                {
                  id: "visionary",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-visionary",
                  },
                  tooltip: {
                    string_id: "mr2022-onboarding-colorway-tooltip-visionary",
                  },
                  description: {
                    string_id:
                      "mr2022-onboarding-colorway-description-visionary",
                  },
                },
                {
                  id: "activist",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-activist",
                  },
                  tooltip: {
                    string_id: "mr2022-onboarding-colorway-tooltip-activist",
                  },
                  description: {
                    string_id:
                      "mr2022-onboarding-colorway-description-activist",
                  },
                },
                {
                  id: "dreamer",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-dreamer",
                  },
                  tooltip: {
                    string_id: "mr2022-onboarding-colorway-tooltip-dreamer",
                  },
                  description: {
                    string_id: "mr2022-onboarding-colorway-description-dreamer",
                  },
                },
                {
                  id: "innovator",
                  label: {
                    string_id: "mr2022-onboarding-colorway-label-innovator",
                  },
                  tooltip: {
                    string_id: "mr2022-onboarding-colorway-tooltip-innovator",
                  },
                  description: {
                    string_id:
                      "mr2022-onboarding-colorway-description-innovator",
                  },
                },
              ],
            },
            primary_button: {
              label: {
                string_id: "mr2022-onboarding-colorway-primary-button-label",
              },
              action: {
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                theme: "automatic",
                navigate: true,
              },
            },
          },
        },
        {
          id: "UPGRADE_MOBILE_DOWNLOAD",
          content: {
            position: "split",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-mobilecrosspromo.svg') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(109.62% 64.62% at 9.75% 62.91%, rgba(103, 51, 205, 0.75) 0%, rgba(0, 108, 207, 0.75) 54.51%, rgba(128, 199, 247, 0.75) 100%)",
            progress_bar: true,
            logo: {},
            title: {
              string_id: "mr2022-onboarding-mobile-download-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-mobile-download-subtitle",
            },
            hero_image: {
              url:
                "chrome://activity-stream/content/data/content/assets/mobile-download-qr-existing-user.svg",
            },
            cta_paragraph: {
              text: {
                string_id: "mr2022-onboarding-mobile-download-cta-text",
                string_name: "download-label",
              },
              action: {
                type: "OPEN_URL",
                data: {
                  args:
                    "https://www.mozilla.org/firefox/mobile/get-app/?utm_medium=firefox-desktop&utm_source=onboarding-modal&utm_campaign=mr2022&utm_content=existing-global",
                  where: "tabshifted",
                },
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-secondary-skip-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
        {
          id: "UPGRADE_PIN_PRIVATE_WINDOW",
          content: {
            position: "split",
            progress_bar: "true",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-pintaskbar.svg') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(83.12% 83.12% at 80.59% 16.88%, rgba(103, 51, 205, 0.75) 0%, rgba(0, 108, 207, 0.75) 54.51%, rgba(128, 199, 247, 0.75) 100%)",
            logo: {},
            title: {
              string_id: "mr2022-upgrade-onboarding-pin-private-window-header",
            },
            subtitle: {
              string_id:
                "mr2022-upgrade-onboarding-pin-private-window-subtitle",
            },
            primary_button: {
              label: {
                string_id:
                  "mr2022-upgrade-onboarding-pin-private-window-primary-button-label",
              },
              action: {
                type: "PIN_FIREFOX_TO_TASKBAR",
                data: {
                  privatePin: true,
                },
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-skip-step-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
        {
          id: "UPGRADE_GRATITUDE",
          content: {
            position: "split",
            progress_bar: "true",
            split_narrow_bkg_position: "-60px",
            background:
              "url('chrome://activity-stream/content/data/content/assets/mr-gratitude.svg') var(--mr-secondary-position) no-repeat, var(--in-content-page-background) radial-gradient(124% 67.28% at 0% 39.91%, rgba(103, 51, 205, 0.75) 0%, rgba(0, 108, 207, 0.75) 54.51%, rgba(128, 199, 247, 0.75) 100%)",
            logo: {},
            title: {
              string_id: "mr2022-onboarding-gratitude-title",
            },
            subtitle: {
              string_id: "mr2022-onboarding-gratitude-subtitle",
            },
            primary_button: {
              label: {
                string_id: "mr2022-onboarding-gratitude-primary-button-label",
              },
              action: {
                type: "OPEN_FIREFOX_VIEW",
                navigate: true,
              },
            },
            secondary_button: {
              label: {
                string_id: "mr2022-onboarding-gratitude-secondary-button-label",
              },
              action: {
                navigate: true,
              },
            },
          },
        },
      ],
    },
  },
  {
    id: "FX_100_UPGRADE",
    template: "spotlight",
    targeting: "false",
    content: {
      template: "multistage",
      id: "FX_100_UPGRADE",
      transitions: true,
      screens: [
        {
          id: "UPGRADE_PIN_FIREFOX",
          content: {
            logo: {
              imageURL:
                "chrome://activity-stream/content/data/content/assets/heart.webp",
              height: "73px",
            },
            has_noodles: true,
            title: {
              fontSize: "36px",
              string_id: "fx100-upgrade-thanks-header",
            },
            title_style: "fancy shine",
            background:
              "url('chrome://activity-stream/content/data/content/assets/confetti.svg') top / 100% no-repeat var(--in-content-page-background)",
            subtitle: {
              string_id: "fx100-upgrade-thanks-keep-body",
            },
            primary_button: {
              label: {
                string_id: "fx100-thank-you-pin-primary-button-label",
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
      ],
    },
  },
  {
    id: "PB_NEWTAB_FOCUS_PROMO",
    type: "default",
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
      promoType: "FOCUS",
      promoHeader: "fluent:about-private-browsing-focus-promo-header-c",
      promoImageLarge: "chrome://browser/content/assets/focus-promo.png",
      promoLinkText: "fluent:about-private-browsing-focus-promo-cta",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle: "fluent:about-private-browsing-focus-promo-text-c",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "SHOW_SPOTLIGHT",
          data: {
            content: {
              id: "FOCUS_PROMO",
              template: "multistage",
              modal: "tab",
              backdrop: "transparent",
              screens: [
                {
                  id: "DEFAULT_MODAL_UI",
                  content: {
                    logo: {
                      imageURL:
                        "chrome://browser/content/assets/focus-logo.svg",
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
                            de:
                              "chrome://browser/content/assets/klar-qr-code.svg",
                          },
                        },
                        marketplace_buttons: ["ios", "android"],
                      },
                    },
                  },
                },
              ],
            },
          },
        },
      },
    },
    priority: 2,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting: "!(region in [ 'DE', 'AT', 'CH'] && localeLanguageCode == 'en')",
  },
  {
    id: "PB_NEWTAB_KLAR_PROMO",
    type: "default",
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
      promoType: "FOCUS",
      promoHeader: "fluent:about-private-browsing-focus-promo-header-c",
      promoImageLarge: "chrome://browser/content/assets/focus-promo.png",
      promoLinkText: "Download Firefox Klar",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle:
        "Firefox Klar clears your history every time while blocking ads and trackers.",
      promoTitleEnabled: true,
      promoButton: {
        action: {
          type: "SHOW_SPOTLIGHT",
          data: {
            content: {
              id: "KLAR_PROMO",
              template: "multistage",
              modal: "tab",
              backdrop: "transparent",
              screens: [
                {
                  id: "DEFAULT_MODAL_UI",
                  order: 0,
                  content: {
                    logo: {
                      imageURL:
                        "chrome://browser/content/assets/focus-logo.svg",
                      height: "48px",
                    },
                    title: "Get Firefox Klar",
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
                            "https://app.adjust.com/a8bxj8j?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fapps.apple.com%2Fde%2Fapp%2Fklar-by-firefox%2Fid1073435754",
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
                            "https://app.adjust.com/a8bxj8j?campaign=firefox-desktop&adgroup=pb&creative=focus-omc172&redirect=https%3A%2F%2Fplay.google.com%2Fstore%2Fapps%2Fdetails%3Fid%3Dorg.mozilla.klar",
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
                            "chrome://browser/content/assets/klar-qr-code.svg",
                          alt_text: {
                            string_id: "spotlight-focus-promo-qr-code",
                          },
                        },
                        marketplace_buttons: ["ios", "android"],
                      },
                    },
                  },
                },
              ],
            },
          },
        },
      },
    },
    priority: 2,
    frequency: {
      custom: [
        {
          cap: 3,
          period: 604800000, // Max 3 per week
        },
      ],
      lifetime: 12,
    },
    targeting: "region in [ 'DE', 'AT', 'CH'] && localeLanguageCode == 'en'",
  },
  {
    id: "PB_NEWTAB_INFO_SECTION",
    template: "pb_newtab",
    content: {
      promoEnabled: false,
      infoEnabled: true,
      infoIcon: "",
      infoTitle: "",
      infoBody: "fluent:about-private-browsing-info-description-private-window",
      infoLinkText: "fluent:about-private-browsing-learn-more-link",
      infoTitleEnabled: false,
    },
    targeting: "true",
  },
  {
    id: "PB_NEWTAB_PIN_PROMO",
    template: "pb_newtab",
    type: "default",
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
      promoHeader: "fluent:about-private-browsing-pin-promo-header",
      promoImageLarge:
        "chrome://browser/content/assets/private-promo-asset.svg",
      promoLinkText: "fluent:about-private-browsing-pin-promo-link-text",
      promoLinkType: "button",
      promoSectionStyle: "below-search",
      promoTitle: "fluent:about-private-browsing-pin-promo-title",
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
                    name:
                      "browser.privacySegmentation.windowSeparation.enabled",
                    value: true,
                  },
                },
              },
              {
                type: "PIN_FIREFOX_TO_TASKBAR",
                data: {
                  privatePin: true,
                },
              },
              {
                type: "BLOCK_MESSAGE",
                data: {
                  id: "PB_NEWTAB_PIN_PROMO",
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
    targeting: "doesAppNeedPrivatePin",
  },
];

const OnboardingMessageProvider = {
  async getExtraAttributes() {
    const [header, button_label] = await L10N.formatMessages([
      { id: "onboarding-welcome-header" },
      { id: "onboarding-start-browsing-button-label" },
    ]);
    return { header: header.value, button_label: button_label.value };
  },
  async getMessages() {
    const messages = await this.translateMessages(await ONBOARDING_MESSAGES());
    return messages;
  },
  async getUntranslatedMessages() {
    // This is helpful for jsonSchema testing - since we are localizing in the provider
    const messages = await ONBOARDING_MESSAGES();
    return messages;
  },
  async translateMessages(messages) {
    let translatedMessages = [];
    for (const msg of messages) {
      let translatedMessage = { ...msg };

      // If the message has no content, do not attempt to translate it
      if (!translatedMessage.content) {
        translatedMessages.push(translatedMessage);
        continue;
      }

      // Translate any secondary buttons separately
      if (msg.content.secondary_button) {
        const [secondary_button_string] = await L10N.formatMessages([
          { id: msg.content.secondary_button.label.string_id },
        ]);
        translatedMessage.content.secondary_button.label =
          secondary_button_string.value;
      }
      if (msg.content.header) {
        const [header_string] = await L10N.formatMessages([
          { id: msg.content.header.string_id },
        ]);
        translatedMessage.content.header = header_string.value;
      }
      translatedMessages.push(translatedMessage);
    }
    return translatedMessages;
  },
  async _doesAppNeedPin(privateBrowsing = false) {
    const needPin = await lazy.ShellService.doesAppNeedPin(privateBrowsing);
    return needPin;
  },
  async _doesAppNeedDefault() {
    let checkDefault = Services.prefs.getBoolPref(
      "browser.shell.checkDefaultBrowser",
      false
    );
    let isDefault = await lazy.ShellService.isDefaultBrowser();
    return checkDefault && !isDefault;
  },
  async getUpgradeMessage() {
    let message = (await OnboardingMessageProvider.getMessages()).find(
      ({ id }) => id === "FX_MR_106_UPGRADE"
    );

    let { content } = message;
    // Helper to find screens and remove them where applicable.
    function removeScreens(check) {
      const { screens } = content;
      for (let i = 0; i < screens?.length; i++) {
        if (check(screens[i])) {
          screens.splice(i--, 1);
        }
      }
    }

    let pinScreen = content.screens?.find(
      screen => screen.id === "UPGRADE_PIN_FIREFOX"
    );
    const needPin = await this._doesAppNeedPin();
    const needDefault = await this._doesAppNeedDefault();
    const needPrivatePin = await this._doesAppNeedPin(true);

    //If a user has Firefox as default remove import screen
    if (!needDefault) {
      removeScreens(screen => screen.id?.startsWith("UPGRADE_IMPORT_SETTINGS"));
    }

    // If already pinned, convert "pin" screen to "welcome" with desired action.
    let removeDefault = !needDefault;
    // If user doesn't need pin, update screen to set "default" or "get started" configuration
    if (!needPin && pinScreen) {
      removeDefault = true;
      let primary = pinScreen.content.primary_button;
      if (needDefault) {
        pinScreen.id = "UPGRADE_ONLY_DEFAULT";
        pinScreen.content.subtitle = {
          string_id: "mr2022-onboarding-existing-set-default-only-subtitle",
        };
        primary.label.string_id =
          "mr2022-onboarding-set-default-primary-button-label";

        // The "pin" screen will now handle "default" so remove other "default."
        primary.action.type = "SET_DEFAULT_BROWSER";
      } else {
        pinScreen.id = "UPGRADE_GET_STARTED";
        pinScreen.content.subtitle = {
          string_id: "mr2022-onboarding-get-started-primary-subtitle",
        };
        primary.label = {
          string_id: "mr2022-onboarding-get-started-primary-button-label",
        };
        delete primary.action.type;
      }
    }

    if (removeDefault) {
      removeScreens(screen => screen.id?.startsWith("UPGRADE_SET_DEFAULT"));
    }

    // Remove mobile download screen if user has sync enabled
    if (lazy.usesFirefoxSync && lazy.mobileDevices > 0) {
      removeScreens(screen => screen.id === "UPGRADE_MOBILE_DOWNLOAD");
    } else if (!lazy.BrowserUtils.sendToDeviceEmailsSupported()) {
      // If send to device emails are not supported for a user's locale,
      // remove the send to device link and update the screen text
      let mobileContent = content.screens.find(
        screen => screen.id === "UPGRADE_MOBILE_DOWNLOAD"
      ).content;
      delete mobileContent.cta_paragraph.action;
      mobileContent.cta_paragraph.text = {
        string_id: "mr2022-onboarding-no-mobile-download-cta-text",
      };
    }

    // If a user has Firefox private pinned remove pin private window screen
    // We also remove standalone pin private window screen if a user doesn't have
    // Firefox pinned in which case the option is shown as checkbox with UPGRADE_PIN_FIREFOX screen
    if (!needPrivatePin || needPin) {
      removeScreens(screen =>
        screen.id?.startsWith("UPGRADE_PIN_PRIVATE_WINDOW")
      );
    }

    // Remove colorways screen if there is no active colorways collection
    const hasActiveColorways = !!lazy.BuiltInThemes.findActiveColorwayCollection?.();
    if (!hasActiveColorways) {
      removeScreens(screen => screen.id?.startsWith("UPGRADE_COLORWAY"));
    }

    return message;
  },
};

const EXPORTED_SYMBOLS = ["OnboardingMessageProvider"];
