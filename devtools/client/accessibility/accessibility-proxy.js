/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

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

    this.accessibilityEventsMap = new Map();
    this.accessibleWalkerEventsMap = new Map();
    this.supports = {};

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
    this._onTargetAvailable = this._onTargetAvailable.bind(this);
  }

  get enabled() {
    return this.accessibilityFront && this.accessibilityFront.enabled;
  }

  /**
   * Indicates whether the accessibility service is enabled.
   */
  get canBeEnabled() {
    // TODO: Just use parentAccessibilityFront after Firefox 75.
    const { canBeEnabled } =
      this.parentAccessibilityFront || this.accessibilityFront;
    return canBeEnabled;
  }

  get currentTarget() {
    return this._currentTarget;
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
      const front = this.accessibleWalkerFront;
      const types =
        filter === FILTERS.ALL ? Object.values(AUDIT_TYPE) : [filter];
      const auditEventHandler = ({ type, ancestries, progress }) => {
        switch (type) {
          case "error":
            this._off(front, "audit-event", auditEventHandler);
            onError();
            resolve();
            break;
          case "completed":
            this._off(front, "audit-event", auditEventHandler);
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

      this._on(front, "audit-event", auditEventHandler);
      front.startAudit({ types });
    });
  }

  /**
   * Stop picking and remove all walker listeners.
   */
  async cancelPick(onHovered, onPicked, onPreviewed, onCanceled) {
    const front = this.accessibleWalkerFront;
    await front.cancelPick();
    this._off(front, "picker-accessible-hovered", onHovered);
    this._off(front, "picker-accessible-picked", onPicked);
    this._off(front, "picker-accessible-previewed", onPreviewed);
    this._off(front, "picker-accessible-canceled", onCanceled);
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
    const front = this.accessibleWalkerFront;
    this._on(front, "picker-accessible-hovered", onHovered);
    this._on(front, "picker-accessible-picked", onPicked);
    this._on(front, "picker-accessible-previewed", onPreviewed);
    this._on(front, "picker-accessible-canceled", onCanceled);
    await front.pick(doFocus);
  }

  async resetAccessiblity() {
    const { enabled } = this.accessibilityFront;
    const { canBeEnabled, canBeDisabled } =
      this.parentAccessibilityFront || this.accessibilityFront;
    return { enabled, canBeDisabled, canBeEnabled };
  }

  startListeningForAccessibilityEvents(eventMap) {
    for (const [type, listener] of Object.entries(eventMap)) {
      this._on(this.accessibleWalkerFront, type, listener);
    }
  }

  stopListeningForAccessibilityEvents(eventMap) {
    for (const [type, listener] of Object.entries(eventMap)) {
      this._off(this.accessibleWalkerFront, type, listener);
    }
  }

  startListeningForLifecycleEvents(eventMap) {
    for (const [type, listeners] of Object.entries(eventMap)) {
      const accessibilityFront =
        // TODO: Remove parentAccessibilityFront check after Firefox 75.
        this.parentAccessibilityFront &&
        PARENT_ACCESSIBILITY_EVENTS.includes(type)
          ? this.parentAccessibilityFront
          : this.accessibilityFront;
      this._on(accessibilityFront, type, listeners);
    }
  }

  stopListeningForLifecycleEvents(eventMap) {
    for (const [type, listeners] of Object.entries(eventMap)) {
      // TODO: Remove parentAccessibilityFront check after Firefox 75.
      const accessibilityFront =
        this.parentAccessibilityFront &&
        PARENT_ACCESSIBILITY_EVENTS.includes(type)
          ? this.parentAccessibilityFront
          : this.accessibilityFront;
      this._off(accessibilityFront, type, listeners);
    }
  }

  /**
   * Part of the proxy initialization only needs to be done when the accessibility panel starts.
   * To avoid performance issues, the panel will explicitly call this method every time a new
   * target becomes available.
   */
  async initializeProxyForPanel(targetFront) {
    await this._updateTarget(targetFront);

    const { mainRoot } = this._currentTarget.client;
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

    // Move front listeners to new front.
    for (const [type, listeners] of this.accessibilityEventsMap.entries()) {
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

    for (const [type, listeners] of this.accessibleWalkerEventsMap.entries()) {
      for (const listener of listeners) {
        this.accessibleWalkerFront.on(type, listener);
      }
    }
  }

  async initialize() {
    try {
      await this.toolbox.targetList.watchTargets(
        [this.toolbox.targetList.TYPES.FRAME],
        this._onTargetAvailable
      );
      // Bug 1602075: auto init feature definition is used for an experiment to
      // determine if we can automatically enable accessibility panel when it
      // opens.
      this.supports.autoInit = Services.prefs.getBoolPref(
        "devtools.accessibility.auto-init.enabled",
        false
      );

      return true;
    } catch (e) {
      // toolbox may be destroyed during this step.
      return false;
    }
  }

  destroy() {
    this.toolbox.targetList.unwatchTargets(
      [this.toolbox.targetList.TYPES.FRAME],
      this._onTargetAvailable
    );

    this.accessibilityEventsMap = null;
    this.accessibleWalkerEventsMap = null;

    this.accessibilityFront = null;
    this.parentAccessibilityFront = null;
    this.accessibleWalkerFront = null;
    this.simulatorFront = null;
    this.simulate = null;
    this.toolbox = null;
  }

  _getEventsMap(front) {
    return front === this.accessibleWalkerFront
      ? this.accessibleWalkerEventsMap
      : this.accessibilityEventsMap;
  }

  async _onTargetAvailable({ targetFront, isTopLevel }) {
    if (isTopLevel) {
      await this._updateTarget(targetFront);
    }
  }

  _on(front, type, listeners) {
    listeners = Array.isArray(listeners) ? listeners : [listeners];

    for (const listener of listeners) {
      front.on(type, listener);
    }

    const eventsMap = this._getEventsMap(front);
    const eventsMapListeners = eventsMap.has(type)
      ? [...eventsMap.get(type), ...listeners]
      : listeners;
    eventsMap.set(type, eventsMapListeners);
  }

  _off(front, type, listeners) {
    listeners = Array.isArray(listeners) ? listeners : [listeners];

    for (const listener of listeners) {
      front.off(type, listener);
    }

    const eventsMap = this._getEventsMap(front);
    if (!eventsMap.has(type)) {
      return;
    }

    const eventsMapListeners = eventsMap
      .get(type)
      .filter(l => !listeners.includes(l));
    if (eventsMapListeners.length) {
      eventsMap.set(type, eventsMapListeners);
    } else {
      eventsMap.delete(type);
    }
  }

  async _updateTarget(targetFront) {
    if (this._updatePromise && this._currentTarget === targetFront) {
      return this._updatePromise;
    }

    this._currentTarget = targetFront;

    this._updatePromise = (async () => {
      this.accessibilityFront = await this._currentTarget.getFront(
        "accessibility"
      );
      // Finalize accessibility front initialization. See accessibility front
      // bootstrap method description.
      await this.accessibilityFront.bootstrap();
      // To add a check for backward compatibility add something similar to the
      // example below:
      //
      // [this.supports.simulation] = await Promise.all([
      //   // Please specify the version of Firefox when the feature was added.
      //   this._currentTarget.actorHasMethod("accessibility", "getSimulator"),
      // ]);
    })();

    return this._updatePromise;
  }
}

exports.AccessibilityProxy = AccessibilityProxy;
