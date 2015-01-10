/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/** Disabled because of Bug 672199
exports["test module exports are frozen"] = function(assert) {
  assert.ok(Object.isFrozen(require("sdk/hotkeys")),
            "module exports are frozen");
};

exports["test redefine exported property"] = function(assert) {
  let hotkeys = require("sdk/hotkeys");
  let { Hotkey } = hotkeys;
  try { Object.defineProperty(hotkeys, 'Hotkey', { value: {} }); } catch(e) {}
  assert.equal(hotkeys.Hotkey, Hotkey, "exports can't be redefined");
};
*/

exports["test can't delete exported property"] = function(assert) {
  let hotkeys = require("sdk/hotkeys");
  let { Hotkey } = hotkeys;

  try { delete hotkeys.Hotkey; } catch(e) {}
  assert.equal(hotkeys.Hotkey, Hotkey, "exports can't be deleted");
};

exports["test can't override exported property"] = function(assert) {
  let hotkeys = require("sdk/hotkeys");
  let { Hotkey } = hotkeys;

  try { hotkeys.Hotkey = Object } catch(e) {}
  assert.equal(hotkeys.Hotkey, Hotkey, "exports can't be overriden");
};

require("sdk/test").run(exports);
