/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");
const { FILTERS } = require("devtools/client/accessibility/constants");

const PARENT_ACCESSIBILITY_EVENTS = [
  "can-be-disabled-change",
  "can-be-enabled-change",
];

/**
 * Component responsible for tracking all Accessibility fronts in parent and
 * content processes.
 */
class AccessibilityProxy {
  constructor(toolbox) {
    this.toolbox = toolbox;

    this.audit = this.audit.bind(this);
    this.disableAccessibility = this.disableAccessibility.bind(this);
    this.enableAccessibility = this.enableAccessibility.bind(this);
    this.getAccessibilityTreeRoot = this.getAccessibilityTreeRoot.bind(this);
    this.resetAccessiblity = this.resetAccessiblity.bind(this);
    this.startListeningForAccessibilityEvents = this.startListeningForAccessibilityEvents.bind(
      this
    );
    this.startListeningForLifecycleEvents = this.startListeningForLifecycleEvents.bind(
      this
    );
    this.stopListeningForAccessibilityEvents = this.stopListeningForAccessibilityEvents.bind(
      this
    );
    this.stopListeningForLifecycleEvents = this.stopListeningForLifecycleEvents.bind(
      this
    );
  }

  get target() {
    return this.toolbox.target;
  }

  get enabled() {
    return this.accessibilityFront && this.accessibilityFront.enabled;
  }

  /**
   * Perform an audit for a given filter.
   *
   * @param  {String} filter
   *         Type of an audit to perform.
   * @param  {Function} onError
   *         Audit error callback.
   * @param  {Function} onProgress
   *         Audit progress callback.
   * @param  {Function} onCompleted
   *         Audit completion callback.
   *
   * @return {Promise}
   *         Resolves when the audit for a top document, that the walker
   *         traverses, completes.
   */
  audit(filter, onError, onProgress, onCompleted) {
    return new Promise(resolve => {
      const types =
        filter === FILTERS.ALL ? Object.values(AUDIT_TYPE) : [filter];
      const auditEventHandler = ({ type, ancestries, progress }) => {
        switch (type) {
          case "error":
            this.accessibleWalkerFront.off("audit-event", auditEventHandler);
            onError();
            resolve();
            break;
          case "completed":
            this.accessibleWalkerFront.off("audit-event", auditEventHandler);
            onCompleted(ancestries);
            resolve();
            break;
          case "progress":
            onProgress(progress);
            break;
          default:
            break;
        }
      };

      this.accessibleWalkerFront.on("audit-event", auditEventHandler);
      this.accessibleWalkerFront.startAudit({ types });
    });
  }

  /**
   * Stop picking and remove all walker listeners.
   */
  async cancelPick(onHovered, onPicked, onPreviewed, onCanceled) {
    await this.accessibleWalkerFront.cancelPick();
    this.accessibleWalkerFront.off("picker-accessible-hovered", onHovered);
    this.accessibleWalkerFront.off("picker-accessible-picked", onPicked);
    this.accessibleWalkerFront.off("picker-accessible-previewed", onPreviewed);
    this.accessibleWalkerFront.off("picker-accessible-canceled", onCanceled);
  }

  async disableAccessibility() {
    // Accessibility service is shut down using the parent accessibility front.
    // That, in turn, shuts down accessibility service in all content processes.
    // We need to wait until that happens to be sure platform  accessibility is
    // fully disabled.
    // TODO: Remove this after Firefox 75 and use parentAccessibilityFront.
    if (this.parentAccessibilityFront) {
      const disabled = this.accessibilityFront.once("shutdown");
      await this.parentAccessibilityFront.disable();
      await disabled;
    } else {
      await this.accessibilityFront.disable();
    }
  }

  async enableAccessibility() {
    // Accessibility service is initialized using the parent accessibility
    // front. That, in turn, initializes accessibility service in all content
    // processes. We need to wait until that happens to be sure platform
    // accessibility is fully enabled.
    // TODO: Remove this after Firefox 75 and use parentAccessibilityFront.
    if (this.parentAccessibilityFront) {
      const enabled = this.accessibilityFront.once("init");
      await this.parentAccessibilityFront.enable();
      await enabled;
    } else {
      await this.accessibilityFront.enable();
    }
  }

  /**
   * Return the topmost level accessibility walker to be used as the root of
   * the accessibility tree view.
   *
   * @return {Object}
   *         Topmost accessibility walker.
   */
  getAccessibilityTreeRoot() {
    return this.accessibleWalkerFront;
  }

  /**
   * Start picking and add walker listeners.
   * @param  {Boolean} doFocus
   *         If true, move keyboard focus into content.
   */
  async pick(doFocus, onHovered, onPicked, onPreviewed, onCanceled) {
    this.accessibleWalkerFront.on("picker-accessible-hovered", onHovered);
    this.accessibleWalkerFront.on("picker-accessible-picked", onPicked);
    this.accessibleWalkerFront.on("picker-accessible-previewed", onPreviewed);
    this.accessibleWalkerFront.on("picker-accessible-canceled", onCanceled);
    await this.accessibleWalkerFront.pick(doFocus);
  }

  async resetAccessiblity() {
    const { enabled } = this.accessibilityFront;
    const { canBeEnabled, canBeDisabled } =
      this.parentAccessibilityFront || this.accessibilityFront;
    return { enabled, canBeDisabled, canBeEnabled };
  }

  startListeningForAccessibilityEvents(eventMap) {
    for (const [type, listener] of Object.entries(eventMap)) {
      this.accessibleWalkerFront.on(type, listener);
    }
  }

  stopListeningForAccessibilityEvents(eventMap) {
    for (const [type, listener] of Object.entries(eventMap)) {
      this.accessibleWalkerFront.off(type, listener);
    }
  }

  startListeningForLifecycleEvents(eventMap) {
    for (let [type, listeners] of Object.entries(eventMap)) {
      listeners = Array.isArray(listeners) ? listeners : [listeners];
      const accessibilityFront =
        // TODO: Remove parentAccessibilityFront check after Firefox 75.
        this.parentAccessibilityFront &&
        PARENT_ACCESSIBILITY_EVENTS.includes(type)
          ? this.parentAccessibilityFront
          : this.accessibilityFront;
      for (const listener of listeners) {
        accessibilityFront.on(type, listener);
      }
    }
  }

  stopListeningForLifecycleEvents(eventMap) {
    for (let [type, listeners] of Object.entries(eventMap)) {
      listeners = Array.isArray(listeners) ? listeners : [listeners];
      // TODO: Remove parentAccessibilityFront check after Firefox 75.
      const accessibilityFront =
        this.parentAccessibilityFront &&
        PARENT_ACCESSIBILITY_EVENTS.includes(type)
          ? this.parentAccessibilityFront
          : this.accessibilityFront;
      for (const listener of listeners) {
        accessibilityFront.off(type, listener);
      }
    }
  }

  async ensureReady() {
    const { mainRoot } = this.target.client;
    if (await mainRoot.hasActor("parentAccessibility")) {
      this.parentAccessibilityFront = await mainRoot.getFront(
        "parentaccessibility"
      );
    }

    this.accessibleWalkerFront = this.accessibilityFront.accessibleWalkerFront;
    this.simulatorFront = this.accessibilityFront.simulatorFront;
    if (this.simulatorFront) {
      this.simulate = types => this.simulatorFront.simulate({ types });
    }
  }

  async initialize() {
    try {
      this.accessibilityFront = await this.target.getFront("accessibility");
      // Finalize accessibility front initialization. See accessibility front
      // bootstrap method description.
      await this.accessibilityFront.bootstrap();
      this.supports = {};
      // To add a check for backward compatibility add something similar to the
      // example below:
      //
      // [this.supports.simulation] = await Promise.all([
      //   // Please specify the version of Firefox when the feature was added.
      //   this.target.actorHasMethod("accessibility", "getSimulator"),
      // ]);
      return true;
    } catch (e) {
      // toolbox may be destroyed during this step.
      return false;
    }
  }

  destroy() {
    this.accessibilityFront = null;
    this.parentAccessibilityFront = null;
    this.accessibleWalkerFront = null;
    this.simulatorFront = null;
    this.simulate = null;
    this.toolbox = null;
  }
}

exports.AccessibilityProxy = AccessibilityProxy;
