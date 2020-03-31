/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* globals Localization */
ChromeUtils.defineModuleGetter(
  this,
  "AttributionCode",
  "resource:///modules/AttributionCode.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonRepository",
  "resource://gre/modules/addons/AddonRepository.jsm"
);
const { FX_MONITOR_OAUTH_CLIENT_ID } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const L10N = new Localization([
  "branding/brand.ftl",
  "browser/branding/brandings.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
]);

const TRAILHEAD_ONBOARDING_TEMPLATE = {
  trigger: { id: "firstRun" },
  template: "trailhead",
  includeBundle: {
    length: 3,
    template: "onboarding",
    trigger: { id: "showOnboarding" },
  },
};

const TRAILHEAD_FULL_PAGE_CONTENT = {
  title: { string_id: "onboarding-welcome-body" },
  learn: {
    text: { string_id: "onboarding-welcome-learn-more" },
    url: "https://www.mozilla.org/firefox/accounts/",
  },
  form: {
    title: { string_id: "onboarding-welcome-form-header" },
    text: { string_id: "onboarding-join-form-body" },
    email: { string_id: "onboarding-fullpage-form-email" },
    button: { string_id: "onboarding-join-form-continue" },
  },
};

const DEFAULT_WELCOME_CONTENT = {
  className: "welcomeCohort",
  benefits: ["sync", "monitor", "lockwise"].map(id => ({
    id,
    title: { string_id: `onboarding-benefit-${id}-title` },
    text: { string_id: `onboarding-benefit-${id}-text` },
  })),
  learn: {
    text: { string_id: "onboarding-welcome-modal-family-learn-more" },
    url: "https://www.mozilla.org/firefox/accounts/",
  },
  form: {
    title: { string_id: "onboarding-welcome-form-header" },
    text: { string_id: "onboarding-join-form-body" },
    email: { string_id: "onboarding-join-form-email" },
    button: { string_id: "onboarding-join-form-continue" },
  },
  skipButton: { string_id: "onboarding-start-browsing-button-label" },
};

const ONBOARDING_MESSAGES = () => [
  {
    id: "TRAILHEAD_1",
    utm_term: "trailhead-join",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      ...DEFAULT_WELCOME_CONTENT,
      title: { string_id: "onboarding-welcome-body" },
    },
  },
  {
    id: "TRAILHEAD_2",
    targeting: "trailheadInterrupt == 'sync'",
    utm_term: "trailhead-sync",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      className: "syncCohort",
      title: { string_id: "onboarding-sync-welcome-header" },
      subtitle: { string_id: "onboarding-sync-welcome-content" },
      benefits: [],
      learn: {
        text: { string_id: "onboarding-sync-welcome-learn-more-link" },
        url: "https://www.mozilla.org/firefox/accounts/",
      },
      form: {
        title: { string_id: "onboarding-sync-form-header" },
        text: { string_id: "onboarding-sync-form-sub-header" },
        email: { string_id: "onboarding-sync-form-input" },
        button: { string_id: "onboarding-sync-form-continue-button" },
      },
      skipButton: { string_id: "onboarding-sync-form-skip-login-button" },
    },
  },
  {
    id: "TRAILHEAD_3",
    targeting: "trailheadInterrupt == 'cards'",
    utm_term: "trailhead-cards",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
  },
  {
    id: "TRAILHEAD_4",
    template: "trailhead",
    targeting: "trailheadInterrupt == 'nofirstrun'",
    trigger: { id: "firstRun" },
  },
  {
    id: "TRAILHEAD_6",
    targeting: "trailheadInterrupt == 'modal_variant_a'",
    utm_term: "trailhead-modal_variant_a",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      ...DEFAULT_WELCOME_CONTENT,
      title: { string_id: "onboarding-welcome-modal-get-body" },
    },
  },
  {
    id: "TRAILHEAD_7",
    targeting: "trailheadInterrupt == 'modal_variant_b'",
    utm_term: "trailhead-modal_variant_b",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      ...DEFAULT_WELCOME_CONTENT,
      title: { string_id: "onboarding-welcome-modal-supercharge-body" },
    },
  },
  {
    id: "TRAILHEAD_8",
    targeting: "trailheadInterrupt == 'modal_variant_c'",
    utm_term: "trailhead-modal_variant_c",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      ...DEFAULT_WELCOME_CONTENT,
      title: { string_id: "onboarding-welcome-modal-privacy-body" },
    },
  },
  {
    id: "FULL_PAGE_1",
    targeting: "trailheadInterrupt == 'full_page_d'",
    utm_term: "trailhead-full_page_d",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      ...TRAILHEAD_FULL_PAGE_CONTENT,
    },
    template: "full_page_interrupt",
  },
  {
    id: "FULL_PAGE_2",
    targeting: "trailheadInterrupt == 'full_page_e'",
    utm_term: "trailhead-full_page_e",
    ...TRAILHEAD_ONBOARDING_TEMPLATE,
    content: {
      className: "fullPageCardsAtTop",
      ...TRAILHEAD_FULL_PAGE_CONTENT,
    },
    template: "full_page_interrupt",
  },
  {
    id: "EXTENDED_TRIPLETS_1",
    template: "extended_triplets",
    campaign: "firstrun_triplets",
    targeting:
      "trailheadTriplet && ((currentDate|date - profileAgeCreated) / 86400000) < 7",
    includeBundle: {
      length: 3,
      template: "onboarding",
      trigger: { id: "showOnboarding" },
    },
    frequency: { lifetime: 5 },
    utm_term: "trailhead-cards",
  },
  {
    id: "TRAILHEAD_CARD_1",
    template: "onboarding",
    bundled: 3,
    order: 3,
    content: {
      title: { string_id: "onboarding-tracking-protection-title2" },
      text: { string_id: "onboarding-tracking-protection-text2" },
      icon: "tracking",
      primary_button: {
        label: { string_id: "onboarding-tracking-protection-button2" },
        action:
          Services.locale.appLocaleAsBCP47.substr(0, 2) === "en"
            ? {
                type: "OPEN_URL",
                data: {
                  args: "https://mzl.la/ETPdefault",
                  where: "tabshifted",
                },
              }
            : {
                type: "OPEN_PREFERENCES_PAGE",
                data: { category: "privacy-trackingprotection" },
              },
      },
    },
    targeting: "trailheadTriplet == 'privacy'",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_2",
    template: "onboarding",
    bundled: 3,
    order: 1,
    content: {
      title: { string_id: "onboarding-data-sync-title" },
      text: { string_id: "onboarding-data-sync-text2" },
      icon: "devices",
      primary_button: {
        label: { string_id: "onboarding-data-sync-button2" },
        action: {
          type: "OPEN_URL",
          addFlowParams: true,
          data: {
            args:
              "https://accounts.firefox.com/?service=sync&action=email&context=fx_desktop_v3&entrypoint=activity-stream-firstrun&style=trailhead",
            where: "tabshifted",
          },
        },
      },
    },
    targeting:
      "(trailheadTriplet in ['supercharge', 'static'] || ( 'dynamic' in trailheadTriplet && usesFirefoxSync == false)) && isChinaRepack == false",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_3",
    template: "onboarding",
    bundled: 3,
    order: 2,
    content: {
      title: { string_id: "onboarding-firefox-monitor-title" },
      text: { string_id: "onboarding-firefox-monitor-text2" },
      icon: "ffmonitor",
      primary_button: {
        label: { string_id: "onboarding-firefox-monitor-button" },
        action: {
          type: "OPEN_URL",
          data: { args: "https://monitor.firefox.com/", where: "tabshifted" },
        },
      },
    },
    // Use service oauth client_id to identify 'Firefox Monitor' service attached to Firefox Account
    // https://docs.telemetry.mozilla.org/datasets/fxa_metrics/attribution.html#service-attribution
    targeting: `(trailheadTriplet in ['supercharge', 'static', 'privacy'] || ('dynamic' in trailheadTriplet && !("${FX_MONITOR_OAUTH_CLIENT_ID}" in attachedFxAOAuthClients|mapToProperty('id')))) && isChinaRepack == false`,
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_4",
    template: "onboarding",
    bundled: 3,
    order: 3,
    content: {
      title: { string_id: "onboarding-browse-privately-title" },
      text: { string_id: "onboarding-browse-privately-text" },
      icon: "private",
      primary_button: {
        label: { string_id: "onboarding-browse-privately-button" },
        action: { type: "OPEN_PRIVATE_BROWSER_WINDOW" },
      },
    },
    targeting: "'dynamic' in trailheadTriplet",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_5",
    template: "onboarding",
    bundled: 3,
    order: 5,
    content: {
      title: { string_id: "onboarding-firefox-send-title" },
      text: { string_id: "onboarding-firefox-send-text2" },
      icon: "ffsend",
      primary_button: {
        label: { string_id: "onboarding-firefox-send-button" },
        action: {
          type: "OPEN_URL",
          data: { args: "https://send.firefox.com/", where: "tabshifted" },
        },
      },
    },
    targeting: "trailheadTriplet == 'payoff' && isChinaRepack == false",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_6",
    template: "onboarding",
    bundled: 3,
    order: 6,
    content: {
      title: { string_id: "onboarding-mobile-phone-title" },
      text: { string_id: "onboarding-mobile-phone-text" },
      icon: "mobile",
      primary_button: {
        label: { string_id: "onboarding-mobile-phone-button" },
        action: {
          type: "OPEN_URL",
          data: {
            args: "https://www.mozilla.org/firefox/mobile/",
            where: "tabshifted",
          },
        },
      },
    },
    targeting:
      "trailheadTriplet in ['supercharge', 'static'] || ('dynamic' in trailheadTriplet && sync.mobileDevices < 1)",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_7",
    template: "onboarding",
    bundled: 3,
    order: 4,
    content: {
      title: { string_id: "onboarding-send-tabs-title" },
      text: { string_id: "onboarding-send-tabs-text2" },
      icon: "sendtab",
      primary_button: {
        label: { string_id: "onboarding-send-tabs-button" },
        action: {
          type: "OPEN_URL",
          data: {
            args:
              "https://support.mozilla.org/kb/send-tab-firefox-desktop-other-devices",
            where: "tabshifted",
          },
        },
      },
    },
    targeting: "'dynamic' in trailheadTriplet",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_8",
    template: "onboarding",
    bundled: 3,
    order: 2,
    content: {
      title: { string_id: "onboarding-pocket-anywhere-title" },
      text: { string_id: "onboarding-pocket-anywhere-text2" },
      icon: "pocket",
      primary_button: {
        label: { string_id: "onboarding-pocket-anywhere-button" },
        action: {
          type: "OPEN_URL",
          data: {
            args: "https://getpocket.com/firefox_learnmore",
            where: "tabshifted",
          },
        },
      },
    },
    targeting: "trailheadTriplet == 'multidevice' && isChinaRepack == false",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_9",
    template: "onboarding",
    bundled: 3,
    order: 7,
    content: {
      title: { string_id: "onboarding-lockwise-strong-passwords-title" },
      text: { string_id: "onboarding-lockwise-strong-passwords-text" },
      icon: "lockwise",
      primary_button: {
        label: { string_id: "onboarding-lockwise-strong-passwords-button" },
        action: {
          type: "OPEN_ABOUT_PAGE",
          data: { args: "logins", where: "tabshifted" },
        },
      },
    },
    targeting: "'dynamic' in trailheadTriplet && isChinaRepack == false",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_10",
    template: "onboarding",
    bundled: 3,
    order: 4,
    content: {
      title: { string_id: "onboarding-facebook-container-title" },
      text: { string_id: "onboarding-facebook-container-text2" },
      icon: "fbcont",
      primary_button: {
        label: { string_id: "onboarding-facebook-container-button" },
        action: {
          type: "OPEN_URL",
          data: {
            args:
              "https://addons.mozilla.org/firefox/addon/facebook-container/",
            where: "tabshifted",
          },
        },
      },
    },
    targeting: "trailheadTriplet == 'payoff' && isChinaRepack == false",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_11",
    template: "onboarding",
    bundled: 3,
    order: 0,
    content: {
      title: { string_id: "onboarding-import-browser-settings-title" },
      text: { string_id: "onboarding-import-browser-settings-text" },
      icon: "import",
      primary_button: {
        label: { string_id: "onboarding-import-browser-settings-button" },
        action: { type: "SHOW_MIGRATION_WIZARD" },
      },
    },
    targeting: "trailheadTriplet == 'dynamic_chrome'",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "TRAILHEAD_CARD_12",
    template: "onboarding",
    bundled: 3,
    order: 1,
    content: {
      title: { string_id: "onboarding-personal-data-promise-title" },
      text: { string_id: "onboarding-personal-data-promise-text" },
      icon: "pledge",
      primary_button: {
        label: { string_id: "onboarding-personal-data-promise-button" },
        action: {
          type: "OPEN_URL",
          data: {
            args: "https://www.mozilla.org/firefox/privacy/",
            where: "tabshifted",
          },
        },
      },
    },
    targeting: "trailheadTriplet == 'privacy'",
    trigger: { id: "showOnboarding" },
  },
  {
    id: "RETURN_TO_AMO_1",
    template: "return_to_amo_overlay",
    content: {
      header: { string_id: "onboarding-welcome-header" },
      title: { string_id: "return-to-amo-sub-header" },
      addon_icon: null,
      icon: "gift-extension",
      text: {
        string_id: "return-to-amo-addon-header",
        args: { "addon-name": null },
      },
      primary_button: {
        label: { string_id: "return-to-amo-extension-button" },
        action: {
          type: "INSTALL_ADDON_FROM_URL",
          data: { url: null, telemetrySource: "rtamo" },
        },
      },
      secondary_button: {
        label: { string_id: "return-to-amo-get-started-button" },
      },
    },
    includeBundle: {
      length: 3,
      template: "onboarding",
      trigger: { id: "showOnboarding" },
    },
    targeting:
      "attributionData.campaign == 'non-fx-button' && attributionData.source == 'addons.mozilla.org'",
    trigger: { id: "firstRun" },
  },
  {
    id: "FXA_ACCOUNTS_BADGE",
    template: "toolbar_badge",
    content: {
      delay: 10000, // delay for 10 seconds
      target: "fxa-toolbar-menu-button",
    },
    // Never accessed the FxA panel && doesn't use Firefox sync & has FxA enabled
    targeting: `!hasAccessedFxAPanel && !usesFirefoxSync && isFxAEnabled == true`,
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

      // We need some addon info if we are showing return to amo overlay, so fetch
      // that, and update the message accordingly
      if (msg.template === "return_to_amo_overlay") {
        try {
          const { name, iconURL, url } = await this.getAddonInfo();
          // If we do not have all the data from the AMO api to indicate to the user
          // what they are installing we don't want to show the message
          if (!name || !iconURL || !url) {
            continue;
          }

          msg.content.text.args["addon-name"] = name;
          msg.content.addon_icon = iconURL;
          msg.content.primary_button.action.data.url = url;
        } catch (e) {
          continue;
        }

        // We know we want to show this message, so translate message strings
        const [
          primary_button_string,
          title_string,
          text_string,
        ] = await L10N.formatMessages([
          { id: msg.content.primary_button.label.string_id },
          { id: msg.content.title.string_id },
          { id: msg.content.text.string_id, args: msg.content.text.args },
        ]);
        translatedMessage.content.primary_button.label =
          primary_button_string.value;
        translatedMessage.content.title = title_string.value;
        translatedMessage.content.text = text_string.value;
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
  async getAddonInfo() {
    try {
      let { content, source } = await AttributionCode.getAttrDataAsync();
      if (!content || source !== "addons.mozilla.org") {
        return null;
      }
      // Attribution data can be double encoded
      while (content.includes("%")) {
        try {
          const result = decodeURIComponent(content);
          if (result === content) {
            break;
          }
          content = result;
        } catch (e) {
          break;
        }
      }
      const [addon] = await AddonRepository.getAddonsByIDs([content]);
      if (addon.sourceURI.scheme !== "https") {
        return null;
      }
      return {
        name: addon.name,
        url: addon.sourceURI.spec,
        iconURL: addon.icons["64"] || addon.icons["32"],
      };
    } catch (e) {
      Cu.reportError(
        "Failed to get the latest add-on version for Return to AMO"
      );
      return null;
    }
  },
};
this.OnboardingMessageProvider = OnboardingMessageProvider;

const EXPORTED_SYMBOLS = ["OnboardingMessageProvider"];
