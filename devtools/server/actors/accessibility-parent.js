/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");
const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

/**
 * A helper class that does all the work related to accessibility service
 * lifecycle (initialization, shutdown, consumer changes, etc) in parent
 * parent process. It is not guaranteed that the AccessibilityActor starts in
 * parent process and thus triggering these lifecycle functions directly is
 * extremely unreliable.
 */
class AccessibilityParent {
  constructor(mm, prefix) {
    this._msgName = `debug:${prefix}accessibility`;
    this.onAccessibilityMessage = this.onAccessibilityMessage.bind(this);
    this.setMessageManager(mm);

    this.userPref = Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED);
    Services.obs.addObserver(this, "a11y-consumers-changed");
    Services.prefs.addObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);

    if (this.enabled && !this.accService) {
      // Set a local reference to an accessibility service if accessibility was
      // started elsewhere to ensure that parent process a11y service does not
      // get GC'ed away.
      this.accService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService);
    }

    this.messageManager.sendAsyncMessage(`${this._msgName}:event`, {
      topic: "initialized",
      data: {
        enabled: this.enabled,
        canBeDisabled: this.canBeDisabled,
        canBeEnabled: this.canBeEnabled
      }
    });
  }

  /**
   * Set up message manager listener to listen for messages coming from the
   * AccessibilityActor when it is instantiated in the child process.
   *
   * @param {Object} mm
   *        Message manager that corresponds to the current content tab.
   */
  setMessageManager(mm) {
    if (this.messageManager === mm) {
      return;
    }

    if (this.messageManager) {
      // If the browser was swapped we need to reset the message manager.
      const oldMM = this.messageManager;
      oldMM.removeMessageListener(this._msgName, this.onAccessibilityMessage);
    }

    this.messageManager = mm;
    if (mm) {
      mm.addMessageListener(this._msgName, this.onAccessibilityMessage);
    }
  }

  /**
   * Content AccessibilityActor message listener.
   *
   * @param  {String} msg
   *         Name of the action to perform.
   */
  onAccessibilityMessage(msg) {
    const { action } = msg.json;
    switch (action) {
      case "enable":
        this.enable();
        break;

      case "disable":
        this.disable();
        break;

      case "disconnect":
        this.destroy();
        break;

      default:
        break;
    }
  }

  observe(subject, topic, data) {
    if (topic === "a11y-consumers-changed") {
      // This event is fired when accessibility service consumers change. Since
      // this observer lives in parent process there are 2 possible consumers of
      // a11y service: XPCOM and PlatformAPI (e.g. screen readers). We only care
      // about PlatformAPI consumer changes because when set, we can no longer
      // disable accessibility service.
      const { PlatformAPI } = JSON.parse(data);
      this.messageManager.sendAsyncMessage(`${this._msgName}:event`, {
        topic: "can-be-disabled-change",
        data: !PlatformAPI
      });
    } else if (!this.disabling && topic === "nsPref:changed" &&
               data === PREF_ACCESSIBILITY_FORCE_DISABLED) {
      // PREF_ACCESSIBILITY_FORCE_DISABLED preference change event. When set to
      // >=1, it means that the user wants to disable accessibility service and
      // prevent it from starting in the future. Note: we also check
      // this.disabling state when handling this pref change because this is how
      // we disable the accessibility inspector itself.
      this.messageManager.sendAsyncMessage(`${this._msgName}:event`, {
        topic: "can-be-enabled-change",
        data: Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED) < 1
      });
    }
  }

  /**
   * A getter that indicates if accessibility service is enabled.
   *
   * @return {Boolean}
   *         True if accessibility service is on.
   */
  get enabled() {
    return Services.appinfo.accessibilityEnabled;
  }

  /**
   * A getter that indicates if the accessibility service can be disabled.
   *
   * @return {Boolean}
   *         True if accessibility service can be disabled.
   */
  get canBeDisabled() {
    if (this.enabled) {
      const a11yService = Cc["@mozilla.org/accessibilityService;1"].getService(
        Ci.nsIAccessibilityService);
      const { PlatformAPI } = JSON.parse(a11yService.getConsumers());
      return !PlatformAPI;
    }

    return true;
  }

  /**
   * A getter that indicates if the accessibility service can be enabled.
   *
   * @return {Boolean}
   *         True if accessibility service can be enabled.
   */
  get canBeEnabled() {
    return Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED) < 1;
  }

  /**
   * Enable accessibility service (via XPCOM service).
   */
  enable() {
    if (this.enabled || !this.canBeEnabled) {
      return;
    }

    this.accService = Cc["@mozilla.org/accessibilityService;1"].getService(
      Ci.nsIAccessibilityService);
  }

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
  }

  /**
   * Destroy thie helper class, remove all listeners and if possible disable
   * accessibility service in the parent process.
   */
  destroy() {
    Services.obs.removeObserver(this, "a11y-consumers-changed");
    Services.prefs.removeObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
    this.setMessageManager(null);
    this.accService = null;
  }
}

/**
 * Setup function that runs in parent process and setups AccessibleActor bits
 * that must always run in parent process.
 *
 * @param  {Object} options.mm
 *         Message manager that corresponds to the current content tab.
 * @param  {String} options.prefix
 *         Unique prefix for message manager messages.
 * @return {Object}
 *         Defines event listeners for when client disconnects or browser gets
 *         swapped.
 */
function setupParentProcess({ mm, prefix }) {
  let accessibility = new AccessibilityParent(mm, prefix);

  return {
    onBrowserSwap: newMM => accessibility.setMessageManager(newMM),
    onDisconnected: () => {
      accessibility.destroy();
      accessibility = null;
    }
  };
}

exports.setupParentProcess = setupParentProcess;
