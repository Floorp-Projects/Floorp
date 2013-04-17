// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function ItemPinHelper(aUnpinnedPrefName) {
  this._prefKey = aUnpinnedPrefName;
}

// Cache preferences on a static variable shared
// by all instances registered to the same pref key.
ItemPinHelper._prefValue = {};

ItemPinHelper.prototype = {
  _getPrefValue: function _getPrefValue() {
    if (ItemPinHelper._prefValue[this._prefKey])
      return ItemPinHelper._prefValue[this._prefKey];

    try {
      // getComplexValue throws if pref never set. Really.
      let prefValue = Services.prefs.getComplexValue(this._prefKey, Ci.nsISupportsString);
      ItemPinHelper._prefValue[this._prefKey] = JSON.parse(prefValue.data);
    } catch(e) {
      ItemPinHelper._prefValue[this._prefKey] = [];
    }

    return ItemPinHelper._prefValue[this._prefKey];
  },

  _setPrefValue: function _setPrefValue(aNewValue) {
    let stringified = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    stringified.data = JSON.stringify(aNewValue);

    Services.prefs.setComplexValue(this._prefKey, Ci.nsISupportsString, stringified);
    ItemPinHelper._prefValue[this._prefKey] = aNewValue;
  },

  isPinned: function isPinned(aItemId) {
    // Bookmarks are visible on StartUI (pinned) by default
    return this._getPrefValue().indexOf(aItemId) === -1;
  },

  setUnpinned: function setPinned(aItemId) {
    let unpinned = this._getPrefValue();
    unpinned.push(aItemId);
    this._setPrefValue(unpinned);
  },

  setPinned: function unsetPinned(aItemId) {
    let unpinned = this._getPrefValue();

    let index = unpinned.indexOf(aItemId);
    unpinned.splice(index, 1);

    this._setPrefValue(unpinned);
  },
}
