/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Interactions"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

/**
 * @typedef {object} DocumentInfo
 *   DocumentInfo is used to store information associated with interactions.
 * @property {string} url
 *   The url of the page that was interacted with.
 */

/**
 * The Interactions object sets up listeners and other approriate tools for
 * obtaining interaction information and passing it to the InteractionsManager.
 */
class _Interactions {
  /**
   * @property {WeakMap<browser, DocumentInfo>} _iteractions
   *   This is used to store active interactions. It is a map of the browser
   *   object to an object containing the url.
   *
   * TODO: How do we ensure interactions are stored in the database even when
   * the browser goes away?
   */
  #interactions = new WeakMap();

  /**
   * Initializes and sets up actors.
   */
  init() {
    if (
      !Services.prefs.getBoolPref("browser.places.interactions.enabled", false)
    ) {
      return;
    }

    this.logConsole = console.createInstance({
      prefix: "InteractionsManager",
      maxLogLevel: Services.prefs.getBoolPref(
        "browser.places.interactions.log",
        false
      )
        ? "Debug"
        : "Warn",
    });

    this.logConsole.debug("init");

    ChromeUtils.registerWindowActor("Interactions", {
      parent: {
        moduleURI: "resource:///actors/InteractionsParent.jsm",
      },
      child: {
        moduleURI: "resource:///actors/InteractionsChild.jsm",
        group: "browsers",
        events: {
          DOMContentLoaded: {},
        },
      },
    });
  }

  /**
   * Registers the start of a new interaction.
   *
   * @param {object} browser
   *   The browser object associated with the interaction.
   * @param {DocumentInfo} docInfo
   *   The document information of the page associated with the interaction.
   */
  registerNewInteraction(browser, docInfo) {
    this.logConsole.debug("New interaction", docInfo);
    this.#interactions.set(browser, docInfo);
  }

  /**
   * Temporary test-only function for obtaining stored interactions, whilst
   * we are still working out the interfaces.
   *
   * @param {object} browser
   *   The browser object associated with the interaction.
   * @returns {DocumentInfo|null}
   *   The document information associated with the browser.
   */
  _getInteractionFor(browser) {
    return this.#interactions.get(browser);
  }
}

const Interactions = new _Interactions();
