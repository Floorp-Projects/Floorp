/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var getID = require("jetpack-id/index");

exports["test Returns GUID when `id` GUID"] = assert => {
  var guid = "{8490ae4f-93bc-13af-80b3-39adf9e7b243}";
  assert.equal(getID({ id: guid }), guid);
};

exports["test Returns domain id when `id` domain id"] = assert => {
  var id = "my-addon@jetpack";
  assert.equal(getID({ id: id }), id);
};

exports["test allows underscores in name"] = assert => {
  var name = "my_addon";
  assert.equal(getID({ name: name }), `@${name}`);
};

exports["test allows underscores in id"] = assert => {
  var id = "my_addon@jetpack";
  assert.equal(getID({ id: id }), id);
};

exports["test Returns valid name when `name` exists"] = assert => {
  var id = "my-addon";
  assert.equal(getID({ name: id }), `@${id}`);
};


exports["test Returns null when `id` and `name` do not exist"] = assert => {
  assert.equal(getID({}), null)
}

exports["test Returns null when no object passed in"] = assert => {
  assert.equal(getID(), null)
}

exports["test Returns null when `id` exists but not GUID/domain"] = assert => {
  var id = "my-addon";
  assert.equal(getID({ id: id }), null);
}

exports["test Returns null when `id` contains multiple @"] = assert => {
  assert.equal(getID({ id: "my@addon@yeah" }), null);
};

exports["test Returns null when `id` or `name` specified in domain format but has invalid characters"] = assert => {
  [" ", "!", "/", "$", "  ", "~", "("].forEach(sym => {
    assert.equal(getID({ id: "my" + sym + "addon@domain" }), null);
    assert.equal(getID({ name: "my" + sym + "addon" }), null);
  });
};

exports["test Returns null, does not crash, when providing non-string properties for `name` and `id`"] = assert => {
  assert.equal(getID({ id: 5 }), null);
  assert.equal(getID({ name: 5 }), null);
  assert.equal(getID({ name: {} }), null);
};

require("sdk/test").run(exports);
