/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  DEFAULT_SITES: "resource://activity-stream/lib/DefaultSites.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  shortURL: "resource://activity-stream/lib/ShortURL.jsm",
  TippyTopProvider: "resource://activity-stream/lib/TippyTopProvider.jsm",
  AboutWelcomeDefaults:
    "resource://activity-stream/aboutwelcome/lib/AboutWelcomeDefaults.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.import(
    "resource://messaging-system/lib/Logger.jsm"
  );
  return new Logger("AboutWelcomeChild");
});

XPCOMUtils.defineLazyGetter(lazy, "tippyTopProvider", () =>
  (async () => {
    const provider = new lazy.TippyTopProvider();
    await provider.init();
    return provider;
  })()
);

const SEARCH_REGION_PREF = "browser.search.region";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "searchRegion",
  SEARCH_REGION_PREF,
  ""
);

/**
 * Lazily get importable sites from parent or reuse cached ones.
 */
function getImportableSites(child) {
  return (
    getImportableSites.cache ??
    (getImportableSites.cache = (async () => {
      // Use tippy top to get packaged rich icons
      const tippyTop = await lazy.tippyTopProvider;
      // Remove duplicate entries if they would appear the same
      return `[${[
        ...new Set(
          (await child.sendQuery("AWPage:IMPORTABLE_SITES")).map(url => {
            // Get both rich icon and short name and save for deduping
            const site = { url };
            tippyTop.processSite(site, "*");
            return JSON.stringify({
              icon: site.tippyTopIcon,
              label: lazy.shortURL(site),
            });
          })
        ),
      ]}]`;
    })())
  );
}

async function getDefaultSites(child) {
  // Get default TopSites by region
  let sites = lazy.DEFAULT_SITES.get(
    lazy.DEFAULT_SITES.has(lazy.searchRegion) ? lazy.searchRegion : ""
  );

  // Use tippy top to get packaged rich icons
  const tippyTop = await lazy.tippyTopProvider;
  let defaultSites = sites.split(",").map(link => {
    let site = { url: link };
    tippyTop.processSite(site);
    return {
      icon: site.tippyTopIcon,
      title: lazy.shortURL(site),
    };
  });
  return Cu.cloneInto(defaultSites, child.contentWindow);
}

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

    Cu.exportFunction(this.AWGetFeatureConfig.bind(this), window, {
      defineAs: "AWGetFeatureConfig",
    });

    Cu.exportFunction(this.AWGetFxAMetricsFlowURI.bind(this), window, {
      defineAs: "AWGetFxAMetricsFlowURI",
    });

    Cu.exportFunction(this.AWGetImportableSites.bind(this), window, {
      defineAs: "AWGetImportableSites",
    });

    Cu.exportFunction(this.AWGetDefaultSites.bind(this), window, {
      defineAs: "AWGetDefaultSites",
    });

    Cu.exportFunction(this.AWGetSelectedTheme.bind(this), window, {
      defineAs: "AWGetSelectedTheme",
    });

    Cu.exportFunction(this.AWGetRegion.bind(this), window, {
      defineAs: "AWGetRegion",
    });

    Cu.exportFunction(this.AWSelectTheme.bind(this), window, {
      defineAs: "AWSelectTheme",
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
      `Loading about:welcome with ${experimentMetadata?.slug ??
        "no"} experiment`
    );

    let featureConfig = lazy.NimbusFeatures.aboutwelcome.getAllVariables();
    featureConfig.needDefault = await this.sendQuery("AWPage:NEED_DEFAULT");
    featureConfig.needPin = await this.sendQuery("AWPage:DOES_APP_NEED_PIN");
    if (featureConfig.languageMismatchEnabled) {
      featureConfig.appAndSystemLocaleInfo = await this.sendQuery(
        "AWPage:GET_APP_AND_SYSTEM_LOCALE_INFO"
      );
    }

    // The MR2022 onboarding variable overrides the about:welcome templateMR
    // variable if enrolled.
    const useMROnboarding = lazy.NimbusFeatures.majorRelease2022.getVariable(
      "onboarding"
    );
    const useTemplateMR = useMROnboarding ?? featureConfig.templateMR;

    // FeatureConfig (from experiments) has higher precendence
    // to defaults. But the `screens` property isn't defined we shouldn't
    // override the default with `null`
    let defaults = lazy.AboutWelcomeDefaults.getDefaults(useTemplateMR);

    const content = await lazy.AboutWelcomeDefaults.prepareContentForReact({
      ...attributionData,
      ...experimentMetadata,
      ...defaults,
      ...featureConfig,
      templateMR: useTemplateMR,
      screens: featureConfig.screens ?? defaults.screens,
    });

    return Cu.cloneInto(content, this.contentWindow);
  }

  AWGetFeatureConfig() {
    return this.wrapPromise(this.getAWContent());
  }

  AWGetFxAMetricsFlowURI() {
    return this.wrapPromise(this.sendQuery("AWPage:FXA_METRICS_FLOW_URI"));
  }

  AWGetImportableSites() {
    return this.wrapPromise(getImportableSites(this));
  }

  AWGetDefaultSites() {
    return this.wrapPromise(getDefaultSites(this));
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
   */
  AWSendToParent(type, data) {
    this.sendAsyncMessage(`AWPage:${type}`, data);
  }

  AWWaitForMigrationClose() {
    return this.wrapPromise(this.sendQuery("AWPage:WAIT_FOR_MIGRATION_CLOSE"));
  }

  AWGetRegion() {
    return this.wrapPromise(this.sendQuery("AWPage:GET_REGION"));
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
