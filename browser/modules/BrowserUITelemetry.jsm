// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserUITelemetry"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
  "resource://gre/modules/UITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

const ALL_BUILTIN_ITEMS = [
  "fullscreen-button",
  "switch-to-metro-button",
];

this.BrowserUITelemetry = {
  init: function() {
    UITelemetry.addSimpleMeasureFunction("toolbars",
                                         this.getToolbarMeasures.bind(this));
    Services.obs.addObserver(this, "browser-delayed-startup-finished", false);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "browser-delayed-startup-finished") {
      this._registerWindow(aSubject);
    }
  },

  /**
   * For the _countableEvents object, constructs a chain of
   * Javascript Objects with the keys in aKeys, with the final
   * key getting the value in aEndWith. If the final key already
   * exists in the final object, its value is not set. In either
   * case, a reference to the second last object in the chain is
   * returned.
   *
   * Example - suppose I want to store:
   * _countableEvents: {
   *   a: {
   *     b: {
   *       c: 0
   *     }
   *   }
   * }
   *
   * And then increment the "c" value by 1, you could call this
   * function like this:
   *
   * let example = this._ensureObjectChain([a, b, c], 0);
   * example["c"]++;
   *
   * Subsequent repetitions of these last two lines would
   * simply result in the c value being incremented again
   * and again.
   *
   * @param aKeys the Array of keys to chain Objects together with.
   * @param aEndWith the value to assign to the last key.
   * @returns a reference to the second last object in the chain -
   *          so in our example, that'd be "b".
   */
  _ensureObjectChain: function(aKeys, aEndWith) {
    let current = this._countableEvents;
    let parent = null;
    for (let [i, key] of Iterator(aKeys)) {
      if (!(key in current)) {
        if (i == aKeys.length - 1) {
          current[key] = aEndWith;
        } else {
          current[key] = {};
        }
      }
      parent = current;
      current = current[key];
    }
    return parent;
  },

  _countableEvents: {},
  _countMouseUpEvent: function(aCategory, aAction, aMouseUpEvent) {
    const BUTTONS = ["left", "middle", "right"];
    let buttonKey = BUTTONS[aMouseUpEvent.button];
    if (buttonKey) {
      let countObject =
        this._ensureObjectChain([aCategory, aAction, buttonKey], 0);
      countObject[buttonKey]++;
    }
  },

  _registerWindow: function(aWindow) {
    aWindow.addEventListener("unload", this);
    let document = aWindow.document;

    for (let areaID of CustomizableUI.areas) {
      let areaNode = document.getElementById(areaID);
      (areaNode.customizationTarget || areaNode).addEventListener("mouseup", this);
    }
  },

  _unregisterWindow: function(aWindow) {
    aWindow.removeEventListener("unload", this);
    let document = aWindow.document;

    for (let areaID of CustomizableUI.areas) {
      let areaNode = document.getElementById(areaID);
      (areaNode.customizationTarget || areaNode).removeEventListener("mouseup", this);
    }
  },

  handleEvent: function(aEvent) {
    switch(aEvent.type) {
      case "unload":
        this._unregisterWindow(aEvent.currentTarget);
        break;
      case "mouseup":
        this._handleMouseUp(aEvent);
        break;
    }
  },

  _handleMouseUp: function(aEvent) {
    let item = aEvent.originalTarget;
    // Perhaps we're seeing one of the default toolbar items
    // being clicked.
    if (ALL_BUILTIN_ITEMS.indexOf(item.id) != -1) {
      // Base case - we clicked directly on one of our built-in items,
      // and we can go ahead and register that click.
      this._countMouseUpEvent("click-builtin-item", item.id, aEvent);
    }
  },

  getToolbarMeasures: function() {
    // Grab the most recent non-popup, non-private browser window for us to
    // analyze the toolbars in...
    let win = RecentWindow.getMostRecentBrowserWindow({
      private: false,
      allowPopups: false
    });

    // If there are no such windows, we're out of luck. :(
    if (!win) {
      return {};
    }

    let document = win.document;
    let result = {};

    result.countableEvents = this._countableEvents;

    return result;
  },
};
