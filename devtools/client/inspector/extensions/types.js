/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

// Helpers injected as props by extension-sidebar.js and used by the
// ObjectInspector component (which is part of the ExpressionResultView).
exports.serviceContainer = {
  highlightDomElement: PropTypes.func.isRequired,
  unHighlightDomElement: PropTypes.func.isRequired,
  openNodeInInspector: PropTypes.func.isRequired,
};
