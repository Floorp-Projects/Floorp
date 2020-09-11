/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* globals Localization */
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

const ONBOARDING_MESSAGES = () => [
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
};
this.OnboardingMessageProvider = OnboardingMessageProvider;

const EXPORTED_SYMBOLS = ["OnboardingMessageProvider"];
