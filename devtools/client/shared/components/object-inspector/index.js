/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const ObjectInspector = require("devtools/client/shared/components/object-inspector/components/ObjectInspector");
const utils = require("devtools/client/shared/components/object-inspector/utils/index");
const reducer = require("devtools/client/shared/components/object-inspector/reducer");
const actions = require("devtools/client/shared/components/object-inspector/actions");

module.exports = { ObjectInspector, utils, actions, reducer };
