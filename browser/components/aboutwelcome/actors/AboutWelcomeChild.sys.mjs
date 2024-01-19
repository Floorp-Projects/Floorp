/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutWelcomeDefaults:
    "resource:///modules/aboutwelcome/AboutWelcomeDefaults.jsm",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("AboutWelcomeChild");
});

export class AboutWelcomeChild extends JSWindowActorChild {
  // Can be used to avoid accesses to the document/contentWindow after it's
  // destroyed, which may throw unhandled exceptions.
  _destroyed = false;

  didDestroy() {
    this._destroyed = true;
  }

  actorCreated() {
    this.exportFunctions();
  }

  /**
   * Send event that can be handled by the page
   *
   * @param {{type: string, data?: any}} action
   */
  sendToPage(action) {
    lazy.log.debug(`Sending to page: ${action.type}`);
    const win = this.document.defaultView;
    const event = new win.CustomEvent("AboutWelcomeChromeToContent", {
      detail: Cu.cloneInto(action, win),
    });
    win.dispatchEvent(event);
  }

  /**
   * Export functions that can be called by page js
   */
  exportFunctions() {
    let window = this.contentWindow;

    Cu.exportFunction(this.AWAddScreenImpression.bind(this), window, {
      defineAs: "AWAddScreenImpression",
    });

    Cu.exportFunction(this.AWGetFeatureConfig.bind(this), window, {
      defineAs: "AWGetFeatureConfig",
    });

    Cu.exportFunction(this.AWGetFxAMetricsFlowURI.bind(this), window, {
      defineAs: "AWGetFxAMetricsFlowURI",
    });

    Cu.exportFunction(this.AWGetSelectedTheme.bind(this), window, {
      defineAs: "AWGetSelectedTheme",
    });

    Cu.exportFunction(this.AWSelectTheme.bind(this), window, {
      defineAs: "AWSelectTheme",
    });

    Cu.exportFunction(this.AWEvaluateScreenTargeting.bind(this), window, {
      defineAs: "AWEvaluateScreenTargeting",
    });

    Cu.exportFunction(this.AWSendEventTelemetry.bind(this), window, {
      defineAs: "AWSendEventTelemetry",
    });

    Cu.exportFunction(this.AWSendToParent.bind(this), window, {
      defineAs: "AWSendToParent",
    });

    Cu.exportFunction(this.AWWaitForMigrationClose.bind(this), window, {
      defineAs: "AWWaitForMigrationClose",
    });

    Cu.exportFunction(this.AWFinish.bind(this), window, {
      defineAs: "AWFinish",
    });

    Cu.exportFunction(this.AWEnsureLangPackInstalled.bind(this), window, {
      defineAs: "AWEnsureLangPackInstalled",
    });

    Cu.exportFunction(
      this.AWNegotiateLangPackForLanguageMismatch.bind(this),
      window,
      {
        defineAs: "AWNegotiateLangPackForLanguageMismatch",
      }
    );

    Cu.exportFunction(this.AWSetRequestedLocales.bind(this), window, {
      defineAs: "AWSetRequestedLocales",
    });

    Cu.exportFunction(this.AWSendToDeviceEmailsSupported.bind(this), window, {
      defineAs: "AWSendToDeviceEmailsSupported",
    });

    Cu.exportFunction(this.AWNewScreen.bind(this), window, {
      defineAs: "AWNewScreen",
    });
  }

  /**
   * Wrap a promise so content can use Promise methods.
   */
  wrapPromise(promise) {
    return new this.contentWindow.Promise((resolve, reject) =>
      promise.then(resolve, reject)
    );
  }

  /**
   * Clones the result of the query into the content window.
   */
  sendQueryAndCloneForContent(...sendQueryArgs) {
    return this.wrapPromise(
      (async () => {
        return Cu.cloneInto(
          await this.sendQuery(...sendQueryArgs),
          this.contentWindow
        );
      })()
    );
  }

  AWSelectTheme(data) {
    return this.wrapPromise(
      this.sendQuery("AWPage:SELECT_THEME", data.toUpperCase())
    );
  }

  AWEvaluateScreenTargeting(data) {
    return this.sendQueryAndCloneForContent(
      "AWPage:EVALUATE_SCREEN_TARGETING",
      data
    );
  }

  AWAddScreenImpression(screen) {
    return this.wrapPromise(
      this.sendQuery("AWPage:ADD_SCREEN_IMPRESSION", screen)
    );
  }

  /**
   * Send initial data to page including experiment information
   */
  async getAWContent() {
    let attributionData = await this.sendQuery("AWPage:GET_ATTRIBUTION_DATA");

    // Return to AMO gets returned early.
    if (attributionData?.template) {
      lazy.log.debug("Loading about:welcome with RTAMO attribution data");
      return Cu.cloneInto(attributionData, this.contentWindow);
    } else if (attributionData?.ua) {
      lazy.log.debug("Loading about:welcome with UA attribution");
    }

    let experimentMetadata =
      lazy.ExperimentAPI.getExperimentMetaData({
        featureId: "aboutwelcome",
      }) || {};

    lazy.log.debug(
      `Loading about:welcome with ${
        experimentMetadata?.slug ?? "no"
      } experiment`
    );

    let featureConfig = lazy.NimbusFeatures.aboutwelcome.getAllVariables();
    featureConfig.needDefault = await this.sendQuery("AWPage:NEED_DEFAULT");
    featureConfig.needPin = await this.sendQuery("AWPage:DOES_APP_NEED_PIN");
    if (featureConfig.languageMismatchEnabled) {
      featureConfig.appAndSystemLocaleInfo = await this.sendQuery(
        "AWPage:GET_APP_AND_SYSTEM_LOCALE_INFO"
      );
    }

    // FeatureConfig (from experiments) has higher precendence
    // to defaults. But the `screens` property isn't defined we shouldn't
    // override the default with `null`
    let defaults = lazy.AboutWelcomeDefaults.getDefaults();

    const content = await lazy.AboutWelcomeDefaults.prepareContentForReact({
      ...attributionData,
      ...experimentMetadata,
      ...defaults,
      ...featureConfig,
      screens: featureConfig.screens ?? defaults.screens,
      backdrop: featureConfig.backdrop ?? defaults.backdrop,
    });

    return Cu.cloneInto(content, this.contentWindow);
  }

  AWGetFeatureConfig() {
    return this.wrapPromise(this.getAWContent());
  }

  AWGetFxAMetricsFlowURI() {
    return this.wrapPromise(this.sendQuery("AWPage:FXA_METRICS_FLOW_URI"));
  }

  AWGetSelectedTheme() {
    return this.wrapPromise(this.sendQuery("AWPage:GET_SELECTED_THEME"));
  }

  /**
   * Send Event Telemetry
   *
   * @param {object} eventData
   */
  AWSendEventTelemetry(eventData) {
    this.AWSendToParent("TELEMETRY_EVENT", {
      ...eventData,
      event_context: {
        ...eventData.event_context,
      },
    });
  }

  /**
   * Send message that can be handled by AboutWelcomeParent.jsm
   *
   * @param {string} type
   * @param {any=} data
   * @returns {Promise<unknown>}
   */
  AWSendToParent(type, data) {
    return this.sendQueryAndCloneForContent(`AWPage:${type}`, data);
  }

  AWWaitForMigrationClose() {
    return this.wrapPromise(this.sendQuery("AWPage:WAIT_FOR_MIGRATION_CLOSE"));
  }

  AWFinish() {
    const shouldFocusNewtabUrlBar =
      lazy.NimbusFeatures.aboutwelcome.getVariable("newtabUrlBarFocus");

    this.contentWindow.location.href = "about:home";
    if (shouldFocusNewtabUrlBar) {
      this.AWSendToParent("SPECIAL_ACTION", {
        type: "FOCUS_URLBAR",
      });
    }
  }

  AWEnsureLangPackInstalled(negotiated, screenContent) {
    const content = Cu.cloneInto(screenContent, {});
    return this.wrapPromise(
      this.sendQuery(
        "AWPage:ENSURE_LANG_PACK_INSTALLED",
        negotiated.langPack
      ).then(() => {
        const formatting = [];
        const l10n = new Localization(
          ["branding/brand.ftl", "browser/newtab/onboarding.ftl"],
          false,
          undefined,
          // Use the system-ish then app then default locale.
          [...negotiated.requestSystemLocales, "en-US"]
        );

        // Add the negotiated language name as args.
        function addMessageArgsAndUseLangPack(obj) {
          for (const value of Object.values(obj)) {
            if (value?.string_id) {
              value.args = {
                ...value.args,
                negotiatedLanguage: negotiated.langPackDisplayName,
              };

              // Expose fluent strings wanting lang pack as raw.
              if (value.useLangPack) {
                formatting.push(
                  l10n.formatValue(value.string_id, value.args).then(raw => {
                    delete value.string_id;
                    value.raw = raw;
                  })
                );
              }
            }
          }
        }
        addMessageArgsAndUseLangPack(content.languageSwitcher);
        addMessageArgsAndUseLangPack(content);
        return Promise.all(formatting).then(() =>
          Cu.cloneInto(content, this.contentWindow)
        );
      })
    );
  }

  AWSetRequestedLocales(requestSystemLocales) {
    return this.sendQueryAndCloneForContent(
      "AWPage:SET_REQUESTED_LOCALES",
      requestSystemLocales
    );
  }

  AWNegotiateLangPackForLanguageMismatch(appAndSystemLocaleInfo) {
    return this.sendQueryAndCloneForContent(
      "AWPage:NEGOTIATE_LANGPACK",
      appAndSystemLocaleInfo
    );
  }

  AWSendToDeviceEmailsSupported() {
    return this.wrapPromise(
      this.sendQuery("AWPage:SEND_TO_DEVICE_EMAILS_SUPPORTED")
    );
  }

  AWNewScreen(screenId) {
    return this.wrapPromise(this.sendQuery("AWPage:NEW_SCREEN", screenId));
  }

  /**
   * @param {{type: string, detail?: any}} event
   * @override
   */
  handleEvent(event) {
    lazy.log.debug(`Received page event ${event.type}`);
  }
}

const OPTIN_DEFAULT = {
  id: "FAKESPOT_OPTIN_DEFAULT",
  template: "multistage",
  backdrop: "transparent",
  aria_role: "alert",
  UTMTerm: "opt-in",
  screens: [
    {
      id: "FS_OPT_IN",
      content: {
        position: "split",
        title: { string_id: "shopping-onboarding-headline" },
        // We set the dynamic subtitle ID below at the same time as the args;
        // to prevent intermittents caused by the args loading too late.
        subtitle: { string_id: "" },
        above_button_content: [
          {
            type: "text",
            text: {
              string_id: "shopping-onboarding-body",
            },
            link_keys: ["learn_more"],
          },
          {
            type: "image",
            url: "chrome://browser/content/shopping/assets/optInLight.avif",
            darkModeImageURL:
              "chrome://browser/content/shopping/assets/optInDark.avif",
            marginInline: "24px",
          },
          {
            type: "text",
            text: {
              string_id:
                "shopping-onboarding-opt-in-privacy-policy-and-terms-of-use3",
            },
            link_keys: ["privacy_policy", "terms_of_use"],
            font_styles: "legal",
          },
        ],
        learn_more: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/review-checker-review-quality?utm_source=review-checker&utm_campaign=learn-more&utm_medium=in-product",
              where: "tab",
            },
          },
        },
        privacy_policy: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/privacy/firefox?utm_source=review-checker&utm_campaign=privacy-policy&utm_medium=in-product&utm_term=opt-in-screen",
              where: "tab",
            },
          },
        },
        terms_of_use: {
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.fakespot.com/terms?utm_source=review-checker&utm_campaign=terms-of-use&utm_medium=in-product",
              where: "tab",
            },
          },
        },
        primary_button: {
          should_focus_button: true,
          label: { string_id: "shopping-onboarding-opt-in-button" },
          action: {
            type: "SET_PREF",
            data: {
              pref: {
                name: "browser.shopping.experience2023.optedIn",
                value: 1,
              },
            },
          },
        },
        additional_button: {
          label: {
            string_id: "shopping-onboarding-not-now-button",
          },
          style: "link",
          flow: "column",
          action: {
            type: "SET_PREF",
            data: {
              pref: {
                name: "browser.shopping.experience2023.active",
                value: false,
              },
            },
          },
        },
      },
    },
  ],
};

const SHOPPING_MICROSURVEY = {
  id: "SHOPPING_MICROSURVEY",
  template: "multistage",
  backdrop: "transparent",
  transitions: true,
  UTMTerm: "survey",
  screens: [
    {
      id: "SHOPPING_MICROSURVEY_SCREEN_1",
      above_button_steps_indicator: true,
      content: {
        position: "split",
        layout: "survey",
        steps_indicator: {
          string_id: "shopping-onboarding-welcome-steps-indicator-label",
        },
        title: {
          string_id: "shopping-survey-headline",
        },
        subtitle: {
          string_id: "shopping-survey-question-one",
        },
        primary_button: {
          label: {
            string_id: "shopping-survey-next-button-label",
            paddingBlock: "5px",
            marginBlock: "0 12px",
          },
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            data: {
              actions: [],
            },
            navigate: true,
          },
          disabled: "hasActiveMultiSelect",
        },
        additional_button: {
          label: {
            string_id: "shopping-survey-terms-link",
          },
          style: "link",
          flow: "column",
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/about/legal/terms/mozilla/?utm_source=review-checker&utm_campaign=terms-of-use-screen-1&utm_medium=in-product",
              where: "tab",
            },
          },
        },
        dismiss_button: {
          action: {
            dismiss: true,
          },
          label: {
            string_id: "shopping-onboarding-dialog-close-button",
          },
        },
        tiles: {
          type: "multiselect",
          style: {
            flexDirection: "column",
            alignItems: "flex-start",
          },
          data: [
            {
              id: "radio-1",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q1-radio-1-label" },
            },
            {
              id: "radio-2",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q1-radio-2-label" },
            },
            {
              id: "radio-3",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q1-radio-3-label" },
            },
            {
              id: "radio-4",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q1-radio-4-label" },
            },
            {
              id: "radio-5",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q1-radio-5-label" },
            },
          ],
        },
      },
    },
    {
      id: "SHOPPING_MICROSURVEY_SCREEN_2",
      above_button_steps_indicator: true,
      content: {
        position: "split",
        layout: "survey",
        steps_indicator: {
          string_id: "shopping-onboarding-welcome-steps-indicator-label",
        },
        title: {
          string_id: "shopping-survey-headline",
        },
        subtitle: {
          string_id: "shopping-survey-question-two",
        },
        primary_button: {
          label: {
            string_id: "shopping-survey-submit-button-label",
            paddingBlock: "5px",
            marginBlock: "0 12px",
          },
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            data: {
              actions: [],
            },
            navigate: true,
          },
          disabled: "hasActiveMultiSelect",
        },
        additional_button: {
          label: {
            string_id: "shopping-survey-terms-link",
          },
          style: "link",
          flow: "column",
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/about/legal/terms/mozilla/?utm_source=review-checker&utm_campaign=terms-of-use-screen-2&utm_medium=in-product",
              where: "tab",
            },
          },
        },
        dismiss_button: {
          action: {
            dismiss: true,
          },
          label: {
            string_id: "shopping-onboarding-dialog-close-button",
          },
        },
        tiles: {
          type: "multiselect",
          style: {
            flexDirection: "column",
            alignItems: "flex-start",
          },
          data: [
            {
              id: "radio-1",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q2-radio-1-label" },
            },
            {
              id: "radio-2",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q2-radio-2-label" },
            },
            {
              id: "radio-3",
              type: "radio",
              group: "radios",
              defaultValue: false,
              label: { string_id: "shopping-survey-q2-radio-3-label" },
            },
          ],
        },
      },
    },
  ],
};

const OPTED_IN_TIME_PREF = "browser.shopping.experience2023.survey.optedInTime";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isSurveySeen",
  "browser.shopping.experience2023.survey.hasSeen",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "pdpVisits",
  "browser.shopping.experience2023.survey.pdpVisits",
  0
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "optedInTime",
  OPTED_IN_TIME_PREF,
  0
);

let optInDynamicContent;
// Limit pref increase to 5 as we don't need to count any higher than that
const MIN_VISITS_TO_SHOW_SURVEY = 5;
// Wait 24 hours after opt in to show survey
const MIN_TIME_AFTER_OPT_IN = 24 * 60 * 60;

export class AboutWelcomeShoppingChild extends AboutWelcomeChild {
  // Static state used to track session in which user opted-in
  static optedInSession = false;

  // Static used to track PDP visits per session for showing survey
  static eligiblePDPvisits = [];

  constructor() {
    super();
    this.surveyEnabled =
      lazy.NimbusFeatures.shopping2023.getVariable("surveyEnabled");

    // Used by tests
    this.resetChildStates = () => {
      AboutWelcomeShoppingChild.eligiblePDPvisits.length = 0;
      AboutWelcomeShoppingChild.optedInSession = false;
    };
  }

  computeEligiblePDPCount(data) {
    // Increment our pref if this isn't a page we've already seen this session
    if (lazy.pdpVisits < MIN_VISITS_TO_SHOW_SURVEY) {
      this.AWSendToParent("SPECIAL_ACTION", {
        type: "SET_PREF",
        data: {
          pref: {
            name: "browser.shopping.experience2023.survey.pdpVisits",
            value: lazy.pdpVisits + 1,
          },
        },
      });
    }

    // Add this product to our list of unique eligible PDPs visited
    // to prevent errors caused by multiple events being fired simultaneously
    AboutWelcomeShoppingChild.eligiblePDPvisits.push(data?.product_id);
  }

  evaluateAndShowSurvey() {
    // Re-evaluate if we should show the survey
    // Render survey if user is opted-in and has met survey seen conditions
    const now = Date.now() / 1000;
    const hasBeen24HrsSinceOptin =
      lazy.optedInTime && now - lazy.optedInTime >= MIN_TIME_AFTER_OPT_IN;

    this.showMicroSurvey =
      this.surveyEnabled &&
      !lazy.isSurveySeen &&
      !AboutWelcomeShoppingChild.optedInSession &&
      lazy.pdpVisits >= MIN_VISITS_TO_SHOW_SURVEY &&
      hasBeen24HrsSinceOptin;

    if (this.showMicroSurvey) {
      this.renderMessage();
    }
  }

  setOptInTime() {
    const now = Date.now() / 1000;
    this.AWSendToParent("SPECIAL_ACTION", {
      type: "SET_PREF",
      data: {
        pref: {
          name: OPTED_IN_TIME_PREF,
          value: now,
        },
      },
    });
  }

  handleEvent(event) {
    // Decide when to show/hide onboarding and survey message
    const { productUrl, showOnboarding, data } = event.detail;

    // Display onboarding if a user hasn't opted-in
    const optInReady = showOnboarding && productUrl;
    if (optInReady) {
      // Render opt-in message
      AboutWelcomeShoppingChild.optedInSession = true;
      this.AWSetProductURL(new URL(productUrl).hostname);
      this.renderMessage();
      return;
    }

    //Store timestamp if user opts in
    if (
      Object.hasOwn(event.detail, "showOnboarding") &&
      !event.detail.showOnboarding &&
      !lazy.optedInTime
    ) {
      this.setOptInTime();
    }
    // Hide the container until the user is eligible to see the survey
    // or user has just completed opt-in
    if (!lazy.isSurveySeen || AboutWelcomeShoppingChild.optedInSession) {
      this.document.getElementById("multi-stage-message-root").hidden = true;
    }

    // Early exit if user has seen survey, if we have no data, encountered
    // an error, or if pdp is ineligible or not unique
    if (
      lazy.isSurveySeen ||
      !data ||
      data.error ||
      !productUrl ||
      (data.needs_analysis &&
        (!data.product_id || !data.grade || !data.adjusted_rating)) ||
      AboutWelcomeShoppingChild.eligiblePDPvisits.includes(data.product_id)
    ) {
      return;
    }

    this.computeEligiblePDPCount(data, productUrl);
    this.evaluateAndShowSurvey();
  }

  renderMessage() {
    this.document.getElementById("multi-stage-message-root").hidden = false;
    this.document.dispatchEvent(
      new this.contentWindow.CustomEvent("RenderWelcome", {
        bubbles: true,
      })
    );
  }

  // TODO - Move messages into an ASRouter message provider. See bug 1848251.
  AWGetFeatureConfig() {
    let messageContent = optInDynamicContent;
    if (this.showMicroSurvey) {
      messageContent = SHOPPING_MICROSURVEY;
      this.setShoppingSurveySeen();
    }
    return Cu.cloneInto(messageContent, this.contentWindow);
  }

  setShoppingSurveySeen() {
    this.AWSendToParent("SPECIAL_ACTION", {
      type: "SET_PREF",
      data: {
        pref: {
          name: "browser.shopping.experience2023.survey.hasSeen",
          value: true,
        },
      },
    });
  }

  // TODO - Add dismiss: true to the primary CTA so it cleans up the React
  // content, which will stop being rendered on opt-in. See bug 1848429.
  AWFinish() {
    if (this._destroyed) {
      return;
    }
    const root = this.document.getElementById("multi-stage-message-root");
    if (root) {
      root.innerHTML = "";
      root
        .appendChild(this.document.createElement("shopping-message-bar"))
        .setAttribute("type", "thank-you-for-feedback");
      this.contentWindow.setTimeout(() => {
        root.hidden = true;
      }, 5000);
    }
  }

  AWSetProductURL(productUrl) {
    let content = JSON.parse(JSON.stringify(OPTIN_DEFAULT));
    const [optInScreen] = content.screens;

    if (productUrl) {
      optInScreen.content.subtitle.string_id =
        "shopping-onboarding-dynamic-subtitle-1";

      switch (
        productUrl // Insert the productUrl into content
      ) {
        case "www.amazon.fr":
        case "www.amazon.de":
          optInScreen.content.subtitle.string_id =
            "shopping-onboarding-single-subtitle";
          optInScreen.content.subtitle.args = {
            currentSite: "Amazon",
          };
          break;
        case "www.amazon.com":
          optInScreen.content.subtitle.args = {
            currentSite: "Amazon",
            secondSite: "Walmart",
            thirdSite: "Best Buy",
          };
          break;
        case "www.walmart.com":
          optInScreen.content.subtitle.args = {
            currentSite: "Walmart",
            secondSite: "Amazon",
            thirdSite: "Best Buy",
          };
          break;
        case "www.bestbuy.com":
          optInScreen.content.subtitle.args = {
            currentSite: "Best Buy",
            secondSite: "Amazon",
            thirdSite: "Walmart",
          };
          break;
        default:
          optInScreen.content.subtitle.args = {
            currentSite: "Amazon",
            secondSite: "Walmart",
            thirdSite: "Best Buy",
          };
      }
    }

    optInDynamicContent = content;
  }

  AWEnsureLangPackInstalled() {}
}
