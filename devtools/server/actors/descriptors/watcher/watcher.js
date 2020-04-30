/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const protocol = require("devtools/shared/protocol");
const { watcherSpec } = require("devtools/shared/specs/watcher");

const ChromeUtils = require("ChromeUtils");
const { registerWatcher, unregisterWatcher } = ChromeUtils.import(
  "resource://devtools/server/actors/descriptors/watcher/FrameWatchers.jsm"
);

exports.WatcherActor = protocol.ActorClassWithSpec(watcherSpec, {
  /**
   * Optionally pass a `browser` in the second argument
   * in order to focus only on targets related to a given <browser> element.
   */
  initialize: function(conn, options) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._browser = options && options.browser;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);

    // Force unwatching for all types, even if we weren't watching.
    // This is fine as unwatchTarget is NOOP if we weren't already watching for this target type.
    this.unwatchTargets("frame");
  },

  form() {
    return {
      actor: this.actorID,
      traits: {
        // FF77+ supports frames in Watcher actor
        frame: true,
      },
    };
  },

  async watchTargets(targetType) {
    // Use DevToolsServerConnection's prefix as a key as we may
    // have multiple clients willing to watch for targets.
    // For example, a Browser Toolbox debugging everything and a Content Toolbox debugging
    // just one tab.
    const { prefix } = this.conn;
    const perPrefixMap =
      Services.ppmm.sharedData.get("DevTools:watchedPerPrefix") || new Map();
    let perPrefixData = perPrefixMap.get(prefix);
    if (!perPrefixData) {
      perPrefixData = {
        targets: new Set(),
        browsingContextID: null,
      };
      perPrefixMap.set(prefix, perPrefixData);
    }
    if (perPrefixData.targets.has(targetType)) {
      throw new Error(`Already watching for '${targetType}' target`);
    }
    perPrefixData.targets.add(targetType);
    if (this._browser) {
      // TODO bug 1625027: update this if we navigate to parent process
      // or <browser> navigates to another BrowsingContext.
      perPrefixData.browsingContextID = this._browser.browsingContext.id;
    }

    Services.ppmm.sharedData.set("DevTools:watchedPerPrefix", perPrefixMap);

    // Flush the data as registerWatcher will indirectly force reading the data
    Services.ppmm.sharedData.flush();

    if (targetType == "frame") {
      // Await the registration in order to ensure receiving the already existing targets
      await registerWatcher(
        this,
        this._browser ? this._browser.browsingContext.id : null
      );
    }
  },

  unwatchTargets(targetType) {
    const perPrefixMap = Services.ppmm.sharedData.get(
      "DevTools:watchedPerPrefix"
    );
    if (!perPrefixMap) {
      return;
    }
    const { prefix } = this.conn;
    const perPrefixData = perPrefixMap.get(prefix);
    if (!perPrefixData) {
      return;
    }
    perPrefixData.targets.delete(targetType);
    if (perPrefixData.targets.size === 0) {
      perPrefixMap.delete(prefix);
    }
    Services.ppmm.sharedData.set("DevTools:watchedPerPrefix", perPrefixMap);
    // Flush the data in order to ensure unregister the target actor from DevToolsFrameChild sooner
    Services.ppmm.sharedData.flush();

    if (targetType == "frame") {
      unregisterWatcher(this);
    }
  },

  /**
   * Called by a Watcher module, whenever a new target is available
   */
  notifyTargetAvailable(actor) {
    this.emit("target-available-form", actor);
  },

  /**
   * Called by a Watcher module, whenever a target has been destroyed
   */
  notifyTargetDestroyed(actor) {
    this.emit("target-destroyed-form", actor);
  },

  getParentBrowsingContextID(browsingContextID) {
    const browsingContext = BrowsingContext.get(browsingContextID);
    if (!browsingContext) {
      throw new Error(
        `BrowsingContext with ID=${browsingContextID} doesn't exist.`
      );
    }
    // Top-level documents of tabs, loaded in a <browser> element expose a null `parent`.
    // i.e. Their BrowsingContext has no parent and is considered top level.
    // But... in the context of the Browser Toolbox, we still consider them as child of the browser window.
    // So, for them, fallback on `embedderWindowGlobal`, which will typically be the WindowGlobal for browser.xhtml.
    if (browsingContext.parent) {
      return browsingContext.parent.id;
    }
    if (browsingContext.embedderWindowGlobal) {
      return browsingContext.embedderWindowGlobal.browsingContext.id;
    }
    return null;
  },
});
