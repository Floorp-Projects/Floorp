/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const memory = require("sdk/deprecated/memory");
const { gc } = require("sdk/test/memory");

exports.testMemory = function(assert) {
  var obj = {};
  memory.track(obj, "testMemory.testObj");

  var objs = memory.getObjects("testMemory.testObj");
  assert.equal(objs[0].weakref.get(), obj);
  obj = null;

  gc().then(function() {
    assert.equal(objs[0].weakref.get(), null);
  });
};

require('sdk/test').run(exports);
