/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeTelemetry"];
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AttributionCode: "resource:///modules/AttributionCode.sys.mjs",
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
  TelemetrySession: "resource://gre/modules/TelemetrySession.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "telemetryClientId", () =>
  lazy.ClientID.getClientID()
);
ChromeUtils.defineLazyGetter(
  lazy,
  "browserSessionId",
  () => lazy.TelemetrySession.getMetadata("").sessionId
);

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  const { Logger } = ChromeUtils.importESModule(
    "resource://messaging-system/lib/Logger.sys.mjs"
  );
  return new Logger("AboutWelcomeTelemetry");
});

class AboutWelcomeTelemetry {
  constructor() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "telemetryEnabled",
      "browser.newtabpage.activity-stream.telemetry",
      false
    );
  }

  /**
   * Attach browser attribution data to a ping payload.
   *
   * It intentionally queries the *cached* attribution data other than calling
   * `getAttrDataAsync()` in order to minimize the overhead here.
   * For the same reason, we are not querying the attribution data from
   * `TelemetryEnvironment.currentEnvironment.settings`.
   *
   * In practice, it's very likely that the attribution data is already read
   * and cached at some point by `AboutWelcomeParent`, so it should be able to
   * read the cached results for the most if not all of the pings.
   */
  _maybeAttachAttribution(ping) {
    const attribution = lazy.AttributionCode.getCachedAttributionData();
    if (attribution && Object.keys(attribution).length) {
      ping.attribution = attribution;
    }
    return ping;
  }

  async _createPing(event) {
    if (event.event_context && typeof event.event_context === "object") {
      event.event_context = JSON.stringify(event.event_context);
    }
    let ping = {
      ...event,
      addon_version: Services.appinfo.appBuildID,
      locale: Services.locale.appLocaleAsBCP47,
      client_id: await lazy.telemetryClientId,
      browser_session_id: lazy.browserSessionId,
    };

    return this._maybeAttachAttribution(ping);
  }

  /**
   * Augment the provided event with some metadata and then send it
   * to the messaging-system's onboarding endpoint.
   *
   * Is sometimes used by non-onboarding events.
   *
   * @param event - an object almost certainly from an onboarding flow (though
   *                there is a case where spotlight may use this, too)
   *                containing a nested structure of data for reporting as
   *                telemetry, as documented in
   * https://firefox-source-docs.mozilla.org/browser/components/newtab/docs/v2-system-addon/data_events.html
   *                Does not have all of its data (`_createPing` will augment
   *                with ids and attribution if available).
   */
  async sendTelemetry(event) {
    if (!this.telemetryEnabled) {
      return;
    }

    const ping = await this._createPing(event);

    try {
      this.submitGleanPingForPing(ping);
    } catch (e) {
      // Though Glean APIs are forbidden to throw, it may be possible that a
      // mismatch between the shape of `ping` and the defined metrics is not
      // adequately handled.
      Glean.messagingSystem.gleanPingForPingFailures.add(1);
    }
  }

  /**
   * Tries to infer appropriate Glean metrics on the "messaging-system" ping,
   * sets them, and submits a "messaging-system" ping.
   *
   * Does not check if telemetry is enabled.
   * (Though Glean will check the global prefs).
   *
   * Note: This is a very unusual use of Glean that is specific to the use-
   *       cases of Messaging System. Please do not copy this pattern.
   */
  submitGleanPingForPing(ping) {
    lazy.log.debug(`Submitting Glean ping for ${JSON.stringify(ping)}`);
    // event.event_context is an object, but it may have been stringified.
    let event_context = ping?.event_context;
    let shopping_callout_impression =
      ping?.message_id?.startsWith("FAKESPOT_CALLOUT") &&
      ping?.event === "IMPRESSION";

    if (typeof event_context === "string") {
      try {
        event_context = JSON.parse(event_context);
        // This code is for directing Shopping component based clicks into
        // the Glean Events ping.
        if (
          event_context?.page === "about:shoppingsidebar" ||
          shopping_callout_impression
        ) {
          this.handleShoppingPings(ping, event_context);
        }
      } catch (e) {
        // The Empty JSON strings and non-objects often provided by the
        // existing telemetry we need to send failing to parse do not fit in
        // the spirit of what this error is meant to capture. Instead, we want
        // to capture when what we got should have been an object,
        // but failed to parse.
        if (event_context.length && event_context.includes("{")) {
          Glean.messagingSystem.eventContextParseError.add(1);
        }
      }
    }

    // We echo certain properties from event_context into their own metrics
    // to aid analysis.
    if (event_context?.reason) {
      Glean.messagingSystem.eventReason.set(event_context.reason);
    }
    if (event_context?.page) {
      Glean.messagingSystem.eventPage.set(event_context.page);
    }
    if (event_context?.source) {
      Glean.messagingSystem.eventSource.set(event_context.source);
    }
    if (event_context?.screen_family) {
      Glean.messagingSystem.eventScreenFamily.set(event_context.screen_family);
    }
    // Screen_index was being coerced into a boolean value
    // which resulted in 0 (first screen index) being ignored.
    if (Number.isInteger(event_context?.screen_index)) {
      Glean.messagingSystem.eventScreenIndex.set(event_context.screen_index);
    }
    if (event_context?.screen_id) {
      Glean.messagingSystem.eventScreenId.set(event_context.screen_id);
    }
    if (event_context?.screen_initials) {
      Glean.messagingSystem.eventScreenInitials.set(
        event_context.screen_initials
      );
    }

    // The event_context is also provided as-is as stringified JSON.
    if (event_context) {
      Glean.messagingSystem.eventContext.set(JSON.stringify(event_context));
    }

    if ("attribution" in ping) {
      for (const [key, value] of Object.entries(ping.attribution)) {
        const camelKey = this._snakeToCamelCase(key);
        try {
          Glean.messagingSystemAttribution[camelKey].set(value);
        } catch (e) {
          // We here acknowledge that we don't know the full breadth of data
          // being collected. Ideally AttributionCode will later centralize
          // definition and reporting of attribution data and we can be rid of
          // this fail-safe for collecting the names of unknown keys.
          Glean.messagingSystemAttribution.unknownKeys[camelKey].add(1);
        }
      }
    }

    // List of keys handled above.
    const handledKeys = ["event_context", "attribution"];

    for (const [key, value] of Object.entries(ping)) {
      if (handledKeys.includes(key)) {
        continue;
      }
      const camelKey = this._snakeToCamelCase(key);
      try {
        // We here acknowledge that even known keys might have non-scalar
        // values. We're pretty sure we handled them all with handledKeys,
        // but we might not have.
        // Ideally this can later be removed after running for a version or two
        // with no values seen in messaging_system.invalid_nested_data
        if (typeof value === "object") {
          Glean.messagingSystem.invalidNestedData[camelKey].add(1);
        } else {
          Glean.messagingSystem[camelKey].set(value);
        }
      } catch (e) {
        // We here acknowledge that we don't know the full breadth of data being
        // collected. Ideally we will later gain that confidence and can remove
        // this fail-safe for collecting the names of unknown keys.
        Glean.messagingSystem.unknownKeys[camelKey].add(1);
        // TODO(bug 1600008): For testing, also record the overall count.
        Glean.messagingSystem.unknownKeyCount.add(1);
      }
    }

    // With all the metrics set, now it's time to submit this ping.
    GleanPings.messagingSystem.submit();
  }

  _snakeToCamelCase(s) {
    return s.toString().replace(/_([a-z])/gi, (_str, group) => {
      return group.toUpperCase();
    });
  }

  handleShoppingPings(ping, event_context) {
    const message_id = ping?.message_id;
    // This function helps direct a shopping ping to the correct Glean event.
    if (message_id.startsWith("FAKESPOT_OPTIN_DEFAULT")) {
      // Onboarding page message IDs are generated, but can reliably be
      // assumed to start in this manner.
      switch (ping?.event) {
        case "CLICK_BUTTON":
          switch (event_context?.source) {
            case "privacy_policy":
              Glean.shopping.surfaceShowPrivacyPolicyClicked.record();
              break;
            case "terms_of_use":
              Glean.shopping.surfaceShowTermsClicked.record();
              break;
            case "primary_button":
              // corresponds to 'Analyze Reviews'
              Glean.shopping.surfaceOptInClicked.record();
              break;
            case "additional_button":
              // corresponds to "Not Now"
              Glean.shopping.surfaceNotNowClicked.record();
              break;
            case "learn_more":
              Glean.shopping.surfaceLearnMoreClicked.record();
              break;
          }
          break;
        case "IMPRESSION":
          Glean.shopping.surfaceOnboardingDisplayed.record({
            configuration: ping?.message_id,
          });
          break;
      }
    }
    if (message_id.startsWith("FAKESPOT_CALLOUT")) {
      Glean.shopping.addressBarFeatureCalloutDisplayed.record({
        configuration: message_id,
      });
    }
  }
}
