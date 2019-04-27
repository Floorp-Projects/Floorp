/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
   * Determine which features are supported based on the version of the server. Also, sync
   * the state of the accessibility front/actor.
   * @return {Promise}
   *         A promise that returns true when accessibility front is fully in sync with
   *         the actor.
   */
  async prepareAccessibility() {
    // We must call a method on an accessibility front here (such as getWalker), in
    // oreder to be able to check actor's backward compatibility via actorHasMethod.
    // See targe.js@getActorDescription for more information.
    try {
      this._walker = await this._accessibility.getWalker();
      this._supports = {};
      // Only works with FF61+ targets
      this._supports.enableDisable =
        await this.target.actorHasMethod("accessibility", "enable");

      if (this._supports.enableDisable) {
        ([
          this._supports.relations,
          this._supports.snapshot,
          this._supports.audit,
          this._supports.hydration,
        ] = await Promise.all([
          this.target.actorHasMethod("accessible", "getRelations"),
          this.target.actorHasMethod("accessible", "snapshot"),
          this.target.actorHasMethod("accessible", "audit"),
          this.target.actorHasMethod("accessible", "hydrate"),
        ]));

        await this._accessibility.bootstrap();
      }

      return true;
    } catch (e) {
      // toolbox may be destroyed during this step.
      return false;
    }
  }

  /**
   * Fully initialize accessibility front. Also add listeners for accessibility
   * service lifecycle events that affect the state of the tool tab highlight.
   * @return {Promise}
   *         A promise for when accessibility front is fully initialized.
   */
  initAccessibility() {
    if (!this._initAccessibility) {
      this._initAccessibility = (async function() {
        await Promise.race([
          this.toolbox.isOpen,
          this.toolbox.once("accessibility-init"),
        ]);

        this._accessibility = await this.target.getFront("accessibility");
        // When target is being destroyed (for example on remoteness change), it
        // destroy accessibility front. In case when a11y is not fully initialized, that
        // may result in unresolved promises.
        const prepared = await Promise.race([
          this.prepareAccessibility(),
          this.target.once("close"), // does not have a value.
        ]);
        // If the target is being destroyed, no need to continue.
        if (!prepared) {
          return;
        }

        this._updateToolHighlight();

        this._accessibility.on("init", this._updateToolHighlight);
        this._accessibility.on("shutdown", this._updateToolHighlight);
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

      this._accessibility.off("init", this._updateToolHighlight);
      this._accessibility.off("shutdown", this._updateToolHighlight);

      await this._walker.destroy();
      this._accessibility = null;
      this._walker = null;
    }.bind(this))();
    return this._destroyingAccessibility;
  }

  /**
   * Set the state of the accessibility tab highlight depending on whether the
   * accessibility service is initialized or shutdown.
   */
  async _updateToolHighlight() {
    const isHighlighted = await this.toolbox.isToolHighlighted("accessibility");
    if (this._accessibility.enabled && !isHighlighted) {
      this.toolbox.highlightTool("accessibility");
    } else if (!this._accessibility.enabled && isHighlighted) {
      this.toolbox.unhighlightTool("accessibility");
    }
  }

  async destroy() {
    await this.destroyAccessibility();
    this.toolbox = null;
  }
}

exports.AccessibilityStartup = AccessibilityStartup;
