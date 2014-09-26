/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { id } = require('sdk/self');
const { getAddonByID } = require('sdk/addon/manager');

exports["test add-on manifest was localized"] = function*(assert) {
  let addon = yield getAddonByID(id);
  assert.equal(addon.name, "title-en", "title was translated");
  assert.equal(addon.description, "description-en", "description was translated");
  assert.equal(addon.creator, "author-en", "author was translated");
  assert.equal(addon.homepageURL, "homepage-en", "homepage was translated");
};

require("sdk/test/runner").runTestsFromModule(module);
