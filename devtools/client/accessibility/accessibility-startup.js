/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  AccessibilityProxy,
} = require("devtools/client/accessibility/accessibility-proxy");

/**
 * Component responsible for all accessibility panel startup steps before the panel is
 * actually opened.
 */
class AccessibilityStartup {
  constructor(toolbox) {
    this.toolbox = toolbox;

    this._updateToolHighlight = this._updateToolHighlight.bind(this);

    // Creates accessibility front.
    this.initAccessibility();
  }

  /**
   * Fully initialize accessibility front. Also add listeners for accessibility
   * service lifecycle events that affect the state of the tool tab highlight.
   * @return {Promise}
   *         A promise for when accessibility front is fully initialized.
   */
  initAccessibility() {
    if (!this._initAccessibility) {
      this._initAccessibility = async function() {
        await Promise.race([
          this.toolbox.isOpen,
          this.toolbox.once("accessibility-init"),
        ]);

        this.accessibilityProxy = new AccessibilityProxy(this.toolbox);
        // When target is being destroyed (for example on remoteness change), it
        // destroy accessibility related fronts. In case when a11y is not fully
        // initialized, that may result in unresolved promises.
        const initialized = await Promise.race([
          this.accessibilityProxy.initialize(),
          this.toolbox.target.once("close"), // does not have a value.
        ]);
        // If the target is being destroyed, no need to continue.
        if (!initialized) {
          return;
        }

        this._updateToolHighlight();
        this.accessibilityProxy.startListeningForLifecycleEvents({
          init: this._updateToolHighlight,
          shutdown: this._updateToolHighlight,
        });
      }.bind(this)();
    }

    return this._initAccessibility;
  }

  /**
   * Destroy accessibility front. Also remove listeners for accessibility service
   * lifecycle events.
   * @return {Promise}
   *         A promise for when accessibility front is fully destroyed.
   */
  destroyAccessibility() {
    if (this._destroyingAccessibility) {
      return this._destroyingAccessibility;
    }

    this._destroyingAccessibility = async function() {
      if (!this.accessibilityProxy) {
        return;
      }

      // Ensure that the accessibility isn't still being initiated, otherwise race
      // conditions in the initialization process can throw errors.
      await this._initAccessibility;

      this.accessibilityProxy.stopListeningForLifecycleEvents({
        init: this._updateToolHighlight,
        shutdown: this._updateToolHighlight,
      });

      this.accessibilityProxy.destroy();
      this.accessibilityProxy = null;
    }.bind(this)();
    return this._destroyingAccessibility;
  }

  /**
   * Set the state of the accessibility tab highlight depending on whether the
   * accessibility service is initialized or shutdown.
   */
  async _updateToolHighlight() {
    // Only update the tab highlighted state when the panel can be
    // enabled/disabled manually.
    if (this.accessibilityProxy.supports.autoInit) {
      return;
    }

    const isHighlighted = await this.toolbox.isToolHighlighted("accessibility");
    if (this.accessibilityProxy.enabled && !isHighlighted) {
      this.toolbox.highlightTool("accessibility");
    } else if (!this.accessibilityProxy.enabled && isHighlighted) {
      this.toolbox.unhighlightTool("accessibility");
    }
  }

  async destroy() {
    await this.destroyAccessibility();
    this.toolbox = null;
  }
}

exports.AccessibilityStartup = AccessibilityStartup;
