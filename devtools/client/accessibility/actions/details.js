/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { UPDATE_DETAILS } = require("devtools/client/accessibility/constants");

/**
 * Update details with the given accessible object.
 *
 * @param {Object} dom walker front
 * @param {Object} accessible front
 */
exports.updateDetails = (domWalker, accessible) => dispatch =>
  Promise.all([
    domWalker.getNodeFromActor(accessible.actorID, [
      "rawAccessible",
      "DOMNode",
    ]),
    accessible.getRelations(),
    accessible.audit(),
    accessible.hydrate(),
  ])
    .then(response => dispatch({ accessible, type: UPDATE_DETAILS, response }))
    .catch(error => dispatch({ accessible, type: UPDATE_DETAILS, error }));
