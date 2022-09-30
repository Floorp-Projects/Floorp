/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const ObjectInspector = require("resource://devtools/client/shared/components/object-inspector/components/ObjectInspector.js");
const utils = require("resource://devtools/client/shared/components/object-inspector/utils/index.js");
const reducer = require("resource://devtools/client/shared/components/object-inspector/reducer.js");
const actions = require("resource://devtools/client/shared/components/object-inspector/actions.js");

module.exports = { ObjectInspector, utils, actions, reducer };
