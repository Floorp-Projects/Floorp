/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Configure enzyme with React 16 adapter.
const Enzyme = require("enzyme");
const Adapter = require("enzyme-adapter-react-16");
Enzyme.configure({ adapter: new Adapter() });

const {
  setMocksInGlobal,
} = require("devtools/client/shared/test-helpers/shared-node-helpers");
setMocksInGlobal();
