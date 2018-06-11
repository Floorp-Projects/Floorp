/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AccessibilityFront } = require("devtools/shared/fronts/accessibility");

/**
 * Component responsible for all accessibility panel startup steps before the panel is
 * actually opened.
 */
class AccessibilityStartup {
  constructor(toolbox) {
    this.toolbox = toolbox;

    this._updateAccessibilityState = this._updateAccessibilityState.bind(this);

    // Creates accessibility front.
    this.initAccessibility();
  }

  get target() {
    return this.toolbox.target;
  }

  /**
   * Get the accessibility front for the toolbox.
   */
  get accessibility() {
    return this._accessibility;
  }

  get walker() {
    return this._walker;
  }

  /**
   * Fully initialize accessibility front. Also add listeners for accessibility
   * service lifecycle events that affect picker state and the state of the tool tab
   * highlight.
   * @return {Promise}
   *         A promise for when accessibility front is fully initialized.
   */
  initAccessibility() {
    if (!this._initAccessibility) {
      this._initAccessibility = (async function() {
        this._accessibility = new AccessibilityFront(this.target.client,
                                                     this.target.form);
        // We must call a method on an accessibility front here (such as getWalker), in
        // oreder to be able to check actor's backward compatibility via actorHasMethod.
        // See targe.js@getActorDescription for more information.
        this._walker = await this._accessibility.getWalker();
        // Only works with FF61+ targets
        this._supportsLatestAccessibility =
          await this.target.actorHasMethod("accessibility", "enable");

        if (this._supportsLatestAccessibility) {
          await this._accessibility.bootstrap();
        }

        this._updateAccessibilityState();
        this._accessibility.on("init", this._updateAccessibilityState);
        this._accessibility.on("shutdown", this._updateAccessibilityState);
      }.bind(this))();
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

    this._destroyingAccessibility = (async function() {
      if (!this._accessibility) {
        return;
      }

      // Ensure that the accessibility isn't still being initiated, otherwise race
      // conditions in the initialization process can throw errors.
      await this._initAccessibility;

      this._accessibility.off("init", this._updateAccessibilityState);
      this._accessibility.off("shutdown", this._updateAccessibilityState);

      await this._walker.destroy();
      await this._accessibility.destroy();
      this._accessibility = null;
      this._walker = null;
    }.bind(this))();
    return this._destroyingAccessibility;
  }

  /**
   * Update states of the accessibility picker and accessibility tab highlight.
   * @return {[type]} [description]
   */
  _updateAccessibilityState() {
    this._updateAccessibilityToolHighlight();
    this._updatePickerButton();
  }

  /**
   * Update picker button state and ensure toolbar is re-rendered correctly.
   */
  _updatePickerButton() {
    this.toolbox.updatePickerButton();
    // Calling setToolboxButtons to make sure toolbar is re-rendered correctly.
    this.toolbox.component.setToolboxButtons(this.toolbox.toolbarButtons);
  }

  /**
   * Set the state of the accessibility tab highlight depending on whether the
   * accessibility service is initialized or shutdown.
   */
  _updateAccessibilityToolHighlight() {
    if (this._accessibility.enabled) {
      this.toolbox.highlightTool("accessibility");
    } else {
      this.toolbox.unhighlightTool("accessibility");
    }
  }

  async destroy() {
    await this.destroyAccessibility();
    this.toolbox = null;
  }
}

exports.AccessibilityStartup = AccessibilityStartup;
