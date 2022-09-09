/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const {
  parentAccessibilitySpec,
} = require("devtools/shared/specs/accessibility");

const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

const ParentAccessibilityActor = ActorClassWithSpec(parentAccessibilitySpec, {
  initialize(conn) {
    Actor.prototype.initialize.call(this, conn);

    this.userPref = Services.prefs.getIntPref(
      PREF_ACCESSIBILITY_FORCE_DISABLED
    );

    if (this.enabled && !this.accService) {
      // Set a local reference to an accessibility service if accessibility was
      // started elsewhere to ensure that parent process a11y service does not
      // get GC'ed away.
      this.accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
    }

    Services.obs.addObserver(this, "a11y-consumers-changed");
    Services.prefs.addObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
  },

  bootstrap() {
    return {
      canBeDisabled: this.canBeDisabled,
      canBeEnabled: this.canBeEnabled,
    };
  },

  observe(subject, topic, data) {
    if (topic === "a11y-consumers-changed") {
      // This event is fired when accessibility service consumers change. Since
      // this observer lives in parent process there are 2 possible consumers of
      // a11y service: XPCOM and PlatformAPI (e.g. screen readers). We only care
      // about PlatformAPI consumer changes because when set, we can no longer
      // disable accessibility service.
      const { PlatformAPI } = JSON.parse(data);
      this.emit("can-be-disabled-change", !PlatformAPI);
    } else if (
      !this.disabling &&
      topic === "nsPref:changed" &&
      data === PREF_ACCESSIBILITY_FORCE_DISABLED
    ) {
      // PREF_ACCESSIBILITY_FORCE_DISABLED preference change event. When set to
      // >=1, it means that the user wants to disable accessibility service and
      // prevent it from starting in the future. Note: we also check
      // this.disabling state when handling this pref change because this is how
      // we disable the accessibility inspector itself.
      this.emit("can-be-enabled-change", this.canBeEnabled);
    }
  },

  /**
   * A getter that indicates if accessibility service is enabled.
   *
   * @return {Boolean}
   *         True if accessibility service is on.
   */
  get enabled() {
    return Services.appinfo.accessibilityEnabled;
  },

  /**
   * A getter that indicates if the accessibility service can be disabled.
   *
   * @return {Boolean}
   *         True if accessibility service can be disabled.
   */
  get canBeDisabled() {
    if (this.enabled) {
      const a11yService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService
      );
      const { PlatformAPI } = JSON.parse(a11yService.getConsumers());
      return !PlatformAPI;
    }

    return true;
  },

  /**
   * A getter that indicates if the accessibility service can be enabled.
   *
   * @return {Boolean}
   *         True if accessibility service can be enabled.
   */
  get canBeEnabled() {
    return Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED) < 1;
  },

  /**
   * Enable accessibility service (via XPCOM service).
   */
  enable() {
    if (this.enabled || !this.canBeEnabled) {
      return;
    }

    this.accService = Cc["@mozilla.org/accessibilityService;1"].getService(
      Ci.nsIAccessibilityService
    );
  },

  /**
   * Force disable accessibility service. This method removes the reference to
   * the XPCOM a11y service object and flips the
   * PREF_ACCESSIBILITY_FORCE_DISABLED preference on and off to shutdown a11y
   * service.
   */
  disable() {
    if (!this.enabled || !this.canBeDisabled) {
      return;
    }

    this.disabling = true;
    this.accService = null;
    // Set PREF_ACCESSIBILITY_FORCE_DISABLED to 1 to force disable
    // accessibility service. This is the only way to guarantee an immediate
    // accessibility service shutdown in all processes. This also prevents
    // accessibility service from starting up in the future.
    Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
    // Set PREF_ACCESSIBILITY_FORCE_DISABLED back to previous default or user
    // set value. This will not start accessibility service until the user
    // activates it again. It simply ensures that accessibility service can
    // start again (when value is below 1).
    Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, this.userPref);
    delete this.disabling;
  },

  /**
   * Destroy the helper class, remove all listeners and if possible disable
   * accessibility service in the parent process.
   */
  destroy() {
    this.disable();
    Actor.prototype.destroy.call(this);
    Services.obs.removeObserver(this, "a11y-consumers-changed");
    Services.prefs.removeObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
  },
});

exports.ParentAccessibilityActor = ParentAccessibilityActor;
