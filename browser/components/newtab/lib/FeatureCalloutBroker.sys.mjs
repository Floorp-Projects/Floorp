/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FeatureCallout: "resource:///modules/FeatureCallout.sys.mjs",
});

/**
 * @typedef {Object} FeatureCalloutOptions
 * @property {Window} win window in which messages will be rendered.
 * @property {{name: String, defaultValue?: String}} [pref] optional pref used
 *   to track progress through a given feature tour. for example:
 *   {
 *     name: "browser.pdfjs.feature-tour",
 *     defaultValue: '{ screen: "FEATURE_CALLOUT_1", complete: false }',
 *   }
 *   or { name: "browser.pdfjs.feature-tour" } (defaultValue is optional)
 * @property {String} [location] string to pass as the page when requesting
 *   messages from ASRouter and sending telemetry.
 * @property {MozBrowser} [browser] <browser> element responsible for the
 *   feature callout. for content pages, this is the browser element that the
 *   callout is being shown in. for chrome, this is the active browser.
 * @property {Function} [cleanup] callback to be invoked when the callout is
 *   removed or the window is unloaded.
 * @property {FeatureCalloutTheme} [theme] optional dynamic color theme.
 */

/** @typedef {import("resource:///modules/FeatureCallout.sys.mjs").FeatureCalloutTheme} FeatureCalloutTheme */

/**
 * @typedef {Object} FeatureCalloutItem
 * @property {lazy.FeatureCallout} callout instance of FeatureCallout.
 * @property {Function} [cleanup] cleanup callback.
 * @property {Boolean} showing whether the callout is currently showing.
 */

export class _FeatureCalloutBroker {
  /**
   * Make a new FeatureCallout instance and store it in the callout map. Also
   * add an unload listener to the window to clean up the callout when the
   * window is unloaded.
   * @param {FeatureCalloutOptions} config
   */
  makeFeatureCallout(config) {
    const { win, pref, location, browser, theme } = config;
    // Use an AbortController to clean up the unload listener in case the
    // callout is cleaned up before the window is unloaded.
    const controller = new AbortController();
    const cleanup = () => {
      this.#calloutMap.delete(win);
      controller.abort();
      config.cleanup?.();
    };
    this.#calloutMap.set(win, {
      callout: new lazy.FeatureCallout({
        win,
        pref,
        location,
        context: "chrome",
        browser,
        listener: this.handleFeatureCalloutCallback.bind(this),
        theme,
      }),
      cleanup,
      showing: false,
    });
    win.addEventListener("unload", cleanup, { signal: controller.signal });
  }

  /**
   * Show a feature callout message. For use by ASRouter, to be invoked when a
   * trigger has matched to a feature_callout message.
   * @param {MozBrowser} browser <browser> element associated with the trigger.
   * @param {Object} message feature_callout message from ASRouter.
   *   @see {@link FeatureCalloutMessages.sys.mjs}
   * @returns {Promise<Boolean>} whether the callout was shown.
   */
  async showFeatureCallout(browser, message) {
    // Only show one callout at a time, across all windows.
    if (this.isCalloutShowing) {
      return false;
    }
    const win = browser.ownerGlobal;
    // Avoid showing feature callouts if a dialog or panel is showing.
    if (
      win.gDialogBox?.dialog ||
      [...win.document.querySelectorAll("panel")].some(p => p.state === "open")
    ) {
      return false;
    }
    const currentCallout = this.#calloutMap.get(win);
    // If a custom callout was previously showing, but is no longer showing,
    // tear down the FeatureCallout instance. We avoid tearing them down when
    // they stop showing because they may be shown again, and we want to avoid
    // the overhead of creating a new FeatureCallout instance. But the custom
    // callout instance may be incompatible with the new ASRouter message, so
    // we tear it down and create a new one.
    if (currentCallout && currentCallout.callout.location !== "chrome") {
      currentCallout.cleanup();
    }
    let item = this.#calloutMap.get(win);
    let callout = item?.callout;
    if (item) {
      // If a callout previously showed in this instance, but the new message's
      // tour_pref_name is different, update the old instance's tour properties.
      callout.teardownFeatureTourProgress();
      if (message.content.tour_pref_name) {
        callout.pref = {
          name: message.content.tour_pref_name,
          defaultValue: message.content.tour_pref_default_value,
        };
        callout.setupFeatureTourProgress();
      } else {
        callout.pref = null;
      }
    } else {
      const options = {
        win,
        location: "chrome",
        browser,
        theme: { preset: "chrome" },
      };
      if (message.content.tour_pref_name) {
        options.pref = {
          name: message.content.tour_pref_name,
          defaultValue: message.content.tour_pref_default_value,
        };
      }
      this.makeFeatureCallout(options);
      item = this.#calloutMap.get(win);
      callout = item.callout;
    }
    // Set this to true for now so that we can't be interrupted by another
    // invocation. We'll set it to false below if it ended up not showing.
    item.showing = true;
    item.showing = await callout.showFeatureCallout(message).catch(() => {
      item.cleanup();
      return false;
    });
    return item.showing;
  }

  /**
   * Make a new FeatureCallout instance specific to a special location, tearing
   * down the existing generic FeatureCallout if it exists, and (if no message
   * is passed) requesting a feature callout message to show. Does nothing if a
   * callout is already in progress. This allows the PDF.js feature tour, which
   * simulates content, to be shown in the chrome window without interfering
   * with chrome feature callouts.
   * @param {FeatureCalloutOptions} config
   * @param {Object} message feature_callout message from ASRouter.
   *   @see {@link FeatureCalloutMessages.sys.mjs}
   * @returns {FeatureCalloutItem|null} the callout item, if one was created.
   */
  showCustomFeatureCallout(config, message) {
    if (this.isCalloutShowing) {
      return null;
    }
    const { win, pref, location } = config;
    const currentCallout = this.#calloutMap.get(win);
    if (currentCallout && currentCallout.location !== location) {
      currentCallout.cleanup();
    }
    let item = this.#calloutMap.get(win);
    let callout = item?.callout;
    if (item) {
      callout.teardownFeatureTourProgress();
      callout.pref = pref;
      if (pref) {
        callout.setupFeatureTourProgress();
      }
    } else {
      this.makeFeatureCallout(config);
      item = this.#calloutMap.get(win);
      callout = item.callout;
    }
    item.showing = true;
    // In this case, callers are not necessarily async, so we don't await.
    callout
      .showFeatureCallout(message)
      .then(showing => {
        item.showing = showing;
      })
      .catch(() => {
        item.cleanup();
        item.showing = false;
      });
    /** @type {FeatureCalloutItem} */
    return item;
  }

  handleFeatureCalloutCallback(win, event, data) {
    switch (event) {
      case "end":
        const item = this.#calloutMap.get(win);
        if (item) {
          item.showing = false;
        }
        break;
    }
  }

  /** @returns {Boolean} whether a callout is currently showing. */
  get isCalloutShowing() {
    return [...this.#calloutMap.values()].some(({ showing }) => showing);
  }

  /** @type {Map<Window, FeatureCalloutItem>} */
  #calloutMap = new Map();
}

export const FeatureCalloutBroker = new _FeatureCalloutBroker();
