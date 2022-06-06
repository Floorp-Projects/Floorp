/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["AboutPrivateBrowsingChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
});

class AboutPrivateBrowsingChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();
    let window = this.contentWindow;

    Cu.exportFunction(this.PrivateBrowsingFeatureConfig.bind(this), window, {
      defineAs: "PrivateBrowsingFeatureConfig",
    });
    Cu.exportFunction(this.PrivateBrowsingRecordClick.bind(this), window, {
      defineAs: "PrivateBrowsingRecordClick",
    });
    Cu.exportFunction(
      this.PrivateBrowsingShouldHideDefault.bind(this),
      window,
      {
        defineAs: "PrivateBrowsingShouldHideDefault",
      }
    );
    Cu.exportFunction(
      this.PrivateBrowsingExposureTelemetry.bind(this),
      window,
      { defineAs: "PrivateBrowsingExposureTelemetry" }
    );
  }

  PrivateBrowsingRecordClick(source) {
    const experiment =
      ExperimentAPI.getExperimentMetaData({
        featureId: "privatebrowsing",
      }) || ExperimentAPI.getExperimentMetaData({ featureId: "pbNewtab" });
    if (experiment) {
      Services.telemetry.recordEvent("aboutprivatebrowsing", "click", source);
    }
    return experiment;
  }

  PrivateBrowsingShouldHideDefault() {
    const config = NimbusFeatures.pbNewtab.getAllVariables() || {};
    return config?.content?.hideDefault;
  }

  PrivateBrowsingExposureTelemetry() {
    NimbusFeatures.pbNewtab.recordExposureEvent({ once: false });
  }

  PrivateBrowsingFeatureConfig() {
    const config = NimbusFeatures.privatebrowsing.getAllVariables() || {};

    NimbusFeatures.privatebrowsing.recordExposureEvent();

    // Format urls if any are defined
    ["infoLinkUrl", "promoLinkUrl"].forEach(key => {
      if (config[key]) {
        config[key] = Services.urlFormatter.formatURL(config[key]);
      }
    });

    return Cu.cloneInto(config, this.contentWindow);
  }
}
