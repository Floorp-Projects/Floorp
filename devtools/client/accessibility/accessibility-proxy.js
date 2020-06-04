/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "CombinedProgress",
  "devtools/client/accessibility/utils/audit",
  true
);

const {
  accessibility: { AUDIT_TYPE },
} = require("devtools/shared/constants");
const { FILTERS } = require("devtools/client/accessibility/constants");

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
    this.startListeningForParentLifecycleEvents = this.startListeningForParentLifecycleEvents.bind(
      this
    );
    this.stopListeningForAccessibilityEvents = this.stopListeningForAccessibilityEvents.bind(
      this
    );
    this.stopListeningForLifecycleEvents = this.stopListeningForLifecycleEvents.bind(
      this
    );
    this.stopListeningForParentLifecycleEvents = this.stopListeningForParentLifecycleEvents.bind(
      this
    );
    this.highlightAccessible = this.highlightAccessible.bind(this);
    this.unhighlightAccessible = this.unhighlightAccessible.bind(this);
    this._onTargetAvailable = this._onTargetAvailable.bind(this);
  }

  get enabled() {
    return this.accessibilityFront && this.accessibilityFront.enabled;
  }

  /**
   * Indicates whether the accessibility service is enabled.
   */
  get canBeEnabled() {
    return this.parentAccessibilityFront.canBeEnabled;
  }

  get currentTarget() {
    return this._currentTarget;
  }

  /**
   * Perform an audit for a given filter.
   *
   * @param  {String} filter
   *         Type of an audit to perform.
   * @param  {Function} onProgress
   *         Audit progress callback.
   *
   * @return {Promise}
   *         Resolves when the audit for every document, that each of the frame
   *         accessibility walkers traverse, completes.
   */
  async audit(filter, onProgress) {
    const types = filter === FILTERS.ALL ? Object.values(AUDIT_TYPE) : [filter];
    const totalFrames = this.toolbox.targetList.getAllTargets([
      this.toolbox.targetList.TYPES.FRAME,
    ]).length;
    const progress = new CombinedProgress({
      onProgress,
      totalFrames,
    });
    const audits = await this.withAllAccessibilityWalkerFronts(
      async accessibleWalkerFront =>
        accessibleWalkerFront.audit({
          types,
          onProgress: progress.onProgressForWalker.bind(
            progress,
            accessibleWalkerFront
          ),
        })
    );

    // Accumulate all audits into a single structure.
    const combinedAudit = { ancestries: [] };
    for (const audit of audits) {
      // If any of the audits resulted in an error, no need to continue.
      if (audit.error) {
        return audit;
      }

      combinedAudit.ancestries.push(...audit.ancestries);
    }

    return combinedAudit;
  }

  async disableAccessibility() {
    // Accessibility service is shut down using the parent accessibility front.
    // That, in turn, shuts down accessibility service in all content processes.
    // We need to wait until that happens to be sure platform  accessibility is
    // fully disabled.
    const disabled = this.accessibilityFront.once("shutdown");
    await this.parentAccessibilityFront.disable();
    await disabled;
  }

  async enableAccessibility() {
    // Accessibility service is initialized using the parent accessibility
    // front. That, in turn, initializes accessibility service in all content
    // processes. We need to wait until that happens to be sure platform
    // accessibility is fully enabled.
    const enabled = this.accessibilityFront.once("init");
    await this.parentAccessibilityFront.enable();
    await enabled;
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
   * Look up accessibility fronts (get an existing one or create a new one) for
   * all existing target fronts and run a task with each one of them.
   * @param {Function} task
   *        Function to execute with each accessiblity front.
   */
  async withAllAccessibilityFronts(taskFn) {
    const accessibilityFronts = await this.toolbox.targetList.getAllFronts(
      this.toolbox.targetList.TYPES.FRAME,
      "accessibility"
    );
    const tasks = [];
    for (const accessibilityFront of accessibilityFronts) {
      tasks.push(taskFn(accessibilityFront));
    }

    return Promise.all(tasks);
  }

  /**
   * Look up accessibility walker fronts (get an existing one or create a new
   * one using accessibility front) for all existing target fronts and run a
   * task with each one of them.
   * @param {Function} task
   *        Function to execute with each accessiblity walker front.
   */
  withAllAccessibilityWalkerFronts(taskFn) {
    return this.withAllAccessibilityFronts(async accessibilityFront => {
      if (!accessibilityFront.accessibleWalkerFront) {
        await accessibilityFront.bootstrap();
      }

      return taskFn(accessibilityFront.accessibleWalkerFront);
    });
  }

  /**
   * Start picking and add walker listeners.
   * @param  {Boolean} doFocus
   *         If true, move keyboard focus into content.
   */
  pick(doFocus, onHovered, onPicked, onPreviewed, onCanceled) {
    return this.withAllAccessibilityWalkerFronts(
      async accessibleWalkerFront => {
        this._on(accessibleWalkerFront, "picker-accessible-hovered", onHovered);
        this._on(accessibleWalkerFront, "picker-accessible-picked", onPicked);
        this._on(
          accessibleWalkerFront,
          "picker-accessible-previewed",
          onPreviewed
        );
        this._on(
          accessibleWalkerFront,
          "picker-accessible-canceled",
          onCanceled
        );
        await accessibleWalkerFront.pick(
          // Only pass doFocus to the top level accessibility walker front.
          doFocus && accessibleWalkerFront.targetFront.isTopLevel
        );
      }
    );
  }

  /**
   * Stop picking and remove all walker listeners.
   */
  cancelPick(onHovered, onPicked, onPreviewed, onCanceled) {
    return this.withAllAccessibilityWalkerFronts(
      async accessibleWalkerFront => {
        await accessibleWalkerFront.cancelPick();
        this._off(
          accessibleWalkerFront,
          "picker-accessible-hovered",
          onHovered
        );
        this._off(accessibleWalkerFront, "picker-accessible-picked", onPicked);
        this._off(
          accessibleWalkerFront,
          "picker-accessible-previewed",
          onPreviewed
        );
        this._off(
          accessibleWalkerFront,
          "picker-accessible-canceled",
          onCanceled
        );
      }
    );
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
      this._on(this.accessibilityFront, type, listeners);
    }
  }

  stopListeningForLifecycleEvents(eventMap) {
    for (const [type, listeners] of Object.entries(eventMap)) {
      this._off(this.accessibilityFront, type, listeners);
    }
  }

  startListeningForParentLifecycleEvents(eventMap) {
    for (const [type, listener] of Object.entries(eventMap)) {
      this.parentAccessibilityFront.on(type, listener);
    }
  }

  stopListeningForParentLifecycleEvents(eventMap) {
    for (const [type, listener] of Object.entries(eventMap)) {
      this.parentAccessibilityFront.off(type, listener);
    }
  }

  highlightAccessible(accessibleFront, options) {
    if (!accessibleFront) {
      return;
    }

    const accessibleWalkerFront = accessibleFront.getParent();
    if (!accessibleWalkerFront) {
      return;
    }

    accessibleWalkerFront
      .highlightAccessible(accessibleFront, options)
      .catch(error => {
        // Only report an error where there's still a toolbox. Ignore cases
        // where toolbox is already destroyed.
        if (this.toolbox) {
          console.error(error);
        }
      });
  }

  unhighlightAccessible(accessibleFront) {
    if (!accessibleFront) {
      return;
    }

    const accessibleWalkerFront = accessibleFront.getParent();
    if (!accessibleWalkerFront) {
      return;
    }

    accessibleWalkerFront.unhighlight().catch(error => {
      // Only report an error where there's still a toolbox. Ignore cases
      // where toolbox is already destroyed.
      if (this.toolbox) {
        console.error(error);
      }
    });
  }

  /**
   * Part of the proxy initialization only needs to be done when the accessibility panel starts.
   * To avoid performance issues, the panel will explicitly call this method every time a new
   * target becomes available.
   */
  async initializeProxyForPanel(targetFront) {
    await this._updateTarget(targetFront);

    // No need to retrieve parent accessibility front since root front does not
    // change.
    if (!this.parentAccessibilityFront) {
      this.parentAccessibilityFront = await this._currentTarget.client.mainRoot.getFront(
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
      for (const listener of listeners) {
        this.accessibilityFront.on(type, listener);
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

  async _onTargetAvailable({ targetFront }) {
    if (targetFront.isTopLevel) {
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
