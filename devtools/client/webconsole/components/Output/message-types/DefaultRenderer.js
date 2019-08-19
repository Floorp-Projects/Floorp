/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");

DefaultRenderer.displayName = "DefaultRenderer";

function DefaultRenderer(props) {
  return dom.div({}, "This message type is not supported yet.");
}

module.exports = DefaultRenderer;
