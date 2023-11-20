/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { RemotePageChild } from "resource://gre/actors/RemotePageChild.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
});

export class AboutPrivateBrowsingChild extends RemotePageChild {
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
    Cu.exportFunction(
      this.PrivateBrowsingPromoExposureTelemetry.bind(this),
      window,
      { defineAs: "PrivateBrowsingPromoExposureTelemetry" }
    );
    Cu.exportFunction(this.FeltPrivacyExposureTelemetry.bind(this), window, {
      defineAs: "FeltPrivacyExposureTelemetry",
    });
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

  PrivateBrowsingPromoExposureTelemetry() {
    lazy.NimbusFeatures.pbNewtab.recordExposureEvent({ once: false });
  }

  FeltPrivacyExposureTelemetry() {
    lazy.NimbusFeatures.feltPrivacy.recordExposureEvent({ once: true });
  }
}
