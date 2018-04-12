/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * LightweightThemeChildListener forwards theme updates from LightweightThemeConsumer to
 * the whitelisted in-content pages
 */
class LightweightThemeChildListener {
  constructor() {
    /**
     * The pages that will receive theme updates
     */
    this.whitelist = new Set([
      "about:home",
      "about:newtab",
      "about:welcome",
    ]);

    /**
     * The last theme data received from LightweightThemeConsumer
     */
    this._lastData = null;
  }

  /**
   * Handles theme updates from the parent process
   * @param message from the parent process.
   */
  receiveMessage({ name, data, target }) {
    if (name == "LightweightTheme:Update") {
      this._lastData = data;
      this._update(data, target.content);
    }
  }

  /**
   * Handles events from the content scope.
   * @param event The received event.
   */
  handleEvent(event) {
    const content = event.originalTarget.defaultView;
    if (content != content.top) {
      return;
    }

    if (event.type == "LightweightTheme:Support") {
      this._update(this._lastData, content);
    }
  }

  /**
   * Checks if a given global is allowed to receive theme updates
   * @param content The global to check against.
   * @returns true if the global is allowed to receive updates, false otherwise.
   */
  _isContentWhitelisted(content) {
    return this.whitelist.has(content.document.documentURI);
  }

  /**
   * Forward the theme data to the page.
   * @param data The theme data to forward
   * @param content The receiving global
   */
  _update(data, content) {
    if (this._isContentWhitelisted(content)) {
      const event = Cu.cloneInto({
        detail: {
          type: "LightweightTheme:Update",
          data,
        },
      }, content);
      content.dispatchEvent(new content.CustomEvent("LightweightTheme:Set",
                                                    event));
    }
  }
}

var EXPORTED_SYMBOLS = ["LightweightThemeChildListener"];
