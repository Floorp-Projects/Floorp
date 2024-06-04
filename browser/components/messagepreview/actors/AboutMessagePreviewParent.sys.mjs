/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { JsonSchema } from "resource://gre/modules/JsonSchema.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CFRPageActions: "resource:///modules/asrouter/CFRPageActions.sys.mjs",
  FeatureCalloutBroker:
    "resource:///modules/asrouter/FeatureCalloutBroker.sys.mjs",
  InfoBar: "resource:///modules/asrouter/InfoBar.sys.mjs",
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
  Spotlight: "resource:///modules/asrouter/Spotlight.sys.mjs",
});

function dispatchCFRAction({ type, data }, browser) {
  if (type === "USER_ACTION") {
    lazy.SpecialMessageActions.handleAction(data, browser);
  }
}

export class AboutMessagePreviewParent extends JSWindowActorParent {
  showInfoBar(message, browser) {
    lazy.InfoBar.showInfoBarMessage(browser, message, dispatchCFRAction);
  }

  showSpotlight(message, browser) {
    lazy.Spotlight.showSpotlightDialog(browser, message, () => {});
  }

  showCFR(message, browser) {
    lazy.CFRPageActions.forceRecommendation(
      browser,
      message,
      dispatchCFRAction
    );
  }

  async showFeatureCallout(message, browser) {
    // For messagePreview, force the trigger && targeting to be something we can show.
    message.trigger.id = "nthTabClosed";
    message.targeting = "true";
    // Check whether or not the callout is showing already, then
    // modify the anchor property of the feature callout to
    // ensure it's something we can show.
    let showing = await lazy.FeatureCalloutBroker.showFeatureCallout(
      browser,
      message
    );
    if (!showing) {
      for (const screen of message.content.screens) {
        let existingAnchors = screen.anchors;
        let fallbackAnchor = { selector: "#star-button-box" };

        if (existingAnchors[0].hasOwnProperty("arrow_position")) {
          fallbackAnchor.arrow_position = "top-center-arrow-end";
        } else {
          fallbackAnchor.panel_position = {
            anchor_attachment: "bottomcenter",
            callout_attachment: "topright",
          };
        }

        screen.anchors = [...existingAnchors, fallbackAnchor];
        console.log("ANCHORS: ", screen.anchors);
      }
      // Try showing again
      await lazy.FeatureCalloutBroker.showFeatureCallout(browser, message);
    }
  }

  async showMessage(data) {
    let message;
    try {
      message = JSON.parse(data);
    } catch (e) {
      console.error("Could not parse message", e);
      return;
    }

    const schema = await fetch(
      "chrome://browser/content/asrouter/schemas/MessagingExperiment.schema.json",
      { credentials: "omit" }
    ).then(rsp => rsp.json());

    const result = JsonSchema.validate(message, schema);
    if (!result.valid) {
      console.error(
        `Invalid message: ${JSON.stringify(result.errors, undefined, 2)}`
      );
    }

    const browser =
      this.browsingContext.topChromeWindow.gBrowser.selectedBrowser;
    switch (message.template) {
      case "infobar":
        this.showInfoBar(message, browser);
        return;
      case "spotlight":
        this.showSpotlight(message, browser);
        return;
      case "cfr_doorhanger":
        this.showCFR(message, browser);
        return;
      case "feature_callout":
        this.showFeatureCallout(message, browser);
        return;
      default:
        console.error(`Unsupported message template ${message.template}`);
    }
  }

  receiveMessage(message) {
    const { name, data } = message;

    switch (name) {
      case "MessagePreview:SHOW_MESSAGE":
        this.showMessage(data);
        break;
      default:
        console.log(`Unexpected event ${name} was not handled.`);
    }
  }
}
