/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerServer } = require("devtools/server/main");
const Services = require("Services");
const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const defer = require("devtools/shared/defer");
const { accessibilitySpec } = require("devtools/shared/specs/accessibility");

loader.lazyRequireGetter(
  this,
  "AccessibleWalkerActor",
  "devtools/server/actors/accessibility/walker",
  true
);
loader.lazyRequireGetter(this, "events", "devtools/shared/event-emitter");

const PREF_ACCESSIBILITY_FORCE_DISABLED = "accessibility.force_disabled";

/**
 * The AccessibilityActor is a top level container actor that initializes
 * accessible walker and is the top-most point of interaction for accessibility
 * tools UI.
 */
const AccessibilityActor = ActorClassWithSpec(accessibilitySpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);

    this.initializedDeferred = defer();

    if (DebuggerServer.isInChildProcess) {
      this._msgName = `debug:${this.conn.prefix}accessibility`;
      // eslint-disable-next-line no-restricted-properties
      this.conn.setupInParent({
        module: "devtools/server/actors/accessibility/accessibility-parent",
        setupParent: "setupParentProcess",
      });

      this.onMessage = this.onMessage.bind(this);
      this.messageManager.addMessageListener(
        `${this._msgName}:event`,
        this.onMessage
      );
    } else {
      this.userPref = Services.prefs.getIntPref(
        PREF_ACCESSIBILITY_FORCE_DISABLED
      );
      Services.obs.addObserver(this, "a11y-consumers-changed");
      Services.prefs.addObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
      this.initializedDeferred.resolve();
    }

    Services.obs.addObserver(this, "a11y-init-or-shutdown");
    this.targetActor = targetActor;
  },

  bootstrap() {
    return this.initializedDeferred.promise.then(() => ({
      enabled: this.enabled,
      canBeEnabled: this.canBeEnabled,
      canBeDisabled: this.canBeDisabled,
    }));
  },

  get enabled() {
    return Services.appinfo.accessibilityEnabled;
  },

  get canBeEnabled() {
    if (DebuggerServer.isInChildProcess) {
      return this._canBeEnabled;
    }

    return Services.prefs.getIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED) < 1;
  },

  get canBeDisabled() {
    if (DebuggerServer.isInChildProcess) {
      return this._canBeDisabled;
    } else if (!this.enabled) {
      return true;
    }

    const { PlatformAPI } = JSON.parse(this.walker.a11yService.getConsumers());
    return !PlatformAPI;
  },

  /**
   * Getter for a message manager that corresponds to a current tab. It is onyl
   * used if the AccessibilityActor runs in the child process.
   *
   * @return {Object}
   *         Message manager that corresponds to the current content tab.
   */
  get messageManager() {
    if (!DebuggerServer.isInChildProcess) {
      throw new Error(
        "Message manager should only be used when actor is in child process."
      );
    }

    return this.conn.parentMessageManager;
  },

  onMessage(msg) {
    const { topic, data } = msg.data;

    switch (topic) {
      case "initialized":
        this._canBeEnabled = data.canBeEnabled;
        this._canBeDisabled = data.canBeDisabled;

        // Sometimes when the tool is reopened content process accessibility service is
        // not shut down yet because GC did not run in that process (though it did in
        // parent process and the service was shut down there). We need to sync the two
        // services if possible.
        if (!data.enabled && this.enabled && data.canBeEnabled) {
          this.messageManager.sendAsyncMessage(this._msgName, {
            action: "enable",
          });
        }

        this.initializedDeferred.resolve();
        break;
      case "can-be-disabled-change":
        this._canBeDisabled = data;
        events.emit(this, "can-be-disabled-change", this.canBeDisabled);
        break;

      case "can-be-enabled-change":
        this._canBeEnabled = data;
        events.emit(this, "can-be-enabled-change", this.canBeEnabled);
        break;

      default:
        break;
    }
  },

  /**
   * Enable acessibility service in the given process.
   */
  async enable() {
    if (this.enabled || !this.canBeEnabled) {
      return;
    }

    const initPromise = this.once("init");

    if (DebuggerServer.isInChildProcess) {
      this.messageManager.sendAsyncMessage(this._msgName, { action: "enable" });
    } else {
      // This executes accessibility service lazy getter and adds accessible
      // events observer.
      this.walker.a11yService;
    }

    await initPromise;
  },

  /**
   * Disable acessibility service in the given process.
   */
  async disable() {
    if (!this.enabled || !this.canBeDisabled) {
      return;
    }

    this.disabling = true;
    const shutdownPromise = this.once("shutdown");
    if (DebuggerServer.isInChildProcess) {
      this.messageManager.sendAsyncMessage(this._msgName, {
        action: "disable",
      });
    } else {
      // Set PREF_ACCESSIBILITY_FORCE_DISABLED to 1 to force disable
      // accessibility service. This is the only way to guarantee an immediate
      // accessibility service shutdown in all processes. This also prevents
      // accessibility service from starting up in the future.
      //
      // TODO: Introduce a shutdown method that is exposed via XPCOM on
      // accessibility service.
      Services.prefs.setIntPref(PREF_ACCESSIBILITY_FORCE_DISABLED, 1);
      // Set PREF_ACCESSIBILITY_FORCE_DISABLED back to previous default or user
      // set value. This will not start accessibility service until the user
      // activates it again. It simply ensures that accessibility service can
      // start again (when value is below 1).
      Services.prefs.setIntPref(
        PREF_ACCESSIBILITY_FORCE_DISABLED,
        this.userPref
      );
    }

    await shutdownPromise;
    delete this.disabling;
  },

  /**
   * Observe Accessibility service init and shutdown events. It relays these
   * events to AccessibilityFront iff the event is fired for the a11y service
   * that lives in the same process.
   *
   * @param  {null} subject
   *         Not used.
   * @param  {String} topic
   *         Name of the a11y service event: "a11y-init-or-shutdown".
   * @param  {String} data
   *         "0" corresponds to shutdown and "1" to init.
   */
  observe(subject, topic, data) {
    if (topic === "a11y-init-or-shutdown") {
      // This event is fired when accessibility service is initialized or shut
      // down. "init" and "shutdown" events are only relayed when the enabled
      // state matches the event (e.g. the event came from the same process as
      // the actor).
      const enabled = data === "1";
      if (enabled && this.enabled) {
        events.emit(this, "init");
      } else if (!enabled && !this.enabled) {
        if (this.walker) {
          this.walker.reset();
        }

        events.emit(this, "shutdown");
      }
    } else if (topic === "a11y-consumers-changed") {
      // This event is fired when accessibility service consumers change. There
      // are 3 possible consumers of a11y service: XPCOM, PlatformAPI (e.g.
      // screen readers) and MainProcess. PlatformAPI consumer can only be set
      // in parent process, and MainProcess consumer can only be set in child
      // process. We only care about PlatformAPI consumer changes because when
      // set, we can no longer disable accessibility service.
      const { PlatformAPI } = JSON.parse(data);
      events.emit(this, "can-be-disabled-change", !PlatformAPI);
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
      events.emit(this, "can-be-enabled-change", this.canBeEnabled);
    }
  },

  /**
   * Get or create AccessibilityWalker actor, similar to WalkerActor.
   *
   * @return {Object}
   *         AccessibleWalkerActor for the current tab.
   */
  getWalker() {
    if (!this.walker) {
      this.walker = new AccessibleWalkerActor(this.conn, this.targetActor);
    }
    return this.walker;
  },

  /**
   * Destroy accessibility service actor. This method also shutsdown
   * accessibility service if possible.
   */
  async destroy() {
    if (this.destroyed) {
      await this.destroyed;
      return;
    }

    let resolver;
    this.destroyed = new Promise(resolve => {
      resolver = resolve;
    });

    if (this.walker) {
      this.walker.reset();
    }

    Services.obs.removeObserver(this, "a11y-init-or-shutdown");
    if (DebuggerServer.isInChildProcess) {
      this.messageManager.removeMessageListener(
        `${this._msgName}:event`,
        this.onMessage
      );
    } else {
      Services.obs.removeObserver(this, "a11y-consumers-changed");
      Services.prefs.removeObserver(PREF_ACCESSIBILITY_FORCE_DISABLED, this);
    }

    Actor.prototype.destroy.call(this);
    this.walker = null;
    this.targetActor = null;
    resolver();
  },
});

exports.AccessibilityActor = AccessibilityActor;
