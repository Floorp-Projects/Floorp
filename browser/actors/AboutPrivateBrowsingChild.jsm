/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["AboutPrivateBrowsingChild"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.jsm",
});

class AboutPrivateBrowsingChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();
    let window = this.contentWindow;

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
    Cu.exportFunction(this.PrivateBrowsingEnableNewLogo.bind(this), window, {
      defineAs: "PrivateBrowsingEnableNewLogo",
    });
    Cu.exportFunction(
      this.PrivateBrowsingExposureTelemetry.bind(this),
      window,
      { defineAs: "PrivateBrowsingExposureTelemetry" }
    );
  }

  PrivateBrowsingRecordClick(source) {
    const experiment = lazy.ExperimentAPI.getExperimentMetaData({
      featureId: "pbNewtab",
    });
    if (experiment) {
      Services.telemetry.recordEvent("aboutprivatebrowsing", "click", source);
    }
    return experiment;
  }

  PrivateBrowsingShouldHideDefault() {
    const config = lazy.NimbusFeatures.pbNewtab.getAllVariables() || {};
    return config?.content?.hideDefault;
  }

  PrivateBrowsingEnableNewLogo() {
    return lazy.NimbusFeatures.majorRelease2022.getVariable(
      "feltPrivacyPBMNewLogo"
    );
  }

  PrivateBrowsingExposureTelemetry() {
    lazy.NimbusFeatures.pbNewtab.recordExposureEvent({ once: false });
  }
}
