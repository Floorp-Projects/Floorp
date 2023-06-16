/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AboutWelcomeDefaults:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeDefaults.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("AboutWelcomeChild");
});

async function getSelectedTheme(child) {
  let activeThemeId = await child.sendQuery("AWPage:GET_SELECTED_THEME");
  return activeThemeId;
}

class AboutWelcomeChild extends JSWindowActorChild {
  actorCreated() {
    this.exportFunctions();
  }

  /**
   * Send event that can be handled by the page
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
    return this.wrapPromise(getSelectedTheme(this));
  }

  /**
   * Send Event Telemetry
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
    this.contentWindow.location.href = "about:home";
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
