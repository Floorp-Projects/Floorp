/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "AboutPrivateBrowsingChild",
  "AboutPrivateBrowsingTelemetryHelper",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
  Services: "resource://gre/modules/Services.jsm",
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
  }

  PrivateBrowsingRecordClick(source) {
    const experiment = ExperimentAPI.getExperimentMetaData({
      featureId: "privatebrowsing",
    });
    if (experiment) {
      Services.telemetry.recordEvent("aboutprivatebrowsing", "click", source);
    }
  }

  PrivateBrowsingFeatureConfig() {
    const config = NimbusFeatures.privatebrowsing.getAllVariables({
      sendExposureEvent: true,
    });

    // Format urls if any are defined
    ["infoLinkUrl", "promoLinkUrl"].forEach(key => {
      if (config[key]) {
        config[key] = Services.urlFormatter.formatURL(config[key]);
      }
    });

    return Cu.cloneInto(config, this.contentWindow);
  }
}
