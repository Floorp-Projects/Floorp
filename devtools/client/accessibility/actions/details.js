/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { UPDATE_DETAILS } = require("../constants");

/**
 * Update details with the given accessible object.
 *
 * @param {Object} dom walker front
 * @param {Object} accessible front
 * @param {Object} list of supported serverside features.
 */
exports.updateDetails = (domWalker, accessible, supports) =>
  dispatch => Promise.all([
    domWalker.getNodeFromActor(accessible.actorID, ["rawAccessible", "DOMNode"]),
    supports.relations ? accessible.getRelations() : [],
    supports.audit ? accessible.audit() : {},
    supports.hydration ? accessible.hydrate() : null,
  ]).then(response => dispatch({ accessible, type: UPDATE_DETAILS, response }))
    .catch(error => dispatch({ accessible, type: UPDATE_DETAILS, error }));
