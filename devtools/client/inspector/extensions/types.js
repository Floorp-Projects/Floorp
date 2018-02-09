/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

// Helpers injected as props by extension-sidebar.js and used by the
// ObjectInspector component (which is part of the ObjectValueGripView).
exports.serviceContainer = {
  createObjectClient: PropTypes.func.isRequired,
  releaseActor: PropTypes.func.isRequired,
  highlightDomElement: PropTypes.func.isRequired,
  unHighlightDomElement: PropTypes.func.isRequired,
  openNodeInInspector: PropTypes.func.isRequired,
};
