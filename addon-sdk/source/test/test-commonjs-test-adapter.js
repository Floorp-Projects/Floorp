/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

exports["test custom `Assert`'s"] = require("./commonjs-test-adapter/asserts");

// Disabling this check since it is not yet supported by jetpack.
// if (module == require.main)
  require("test").run(exports);
