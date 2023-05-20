/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  FETCH_CHILDREN,
  SELECT,
  HIGHLIGHT,
  UNHIGHLIGHT,
} = require("resource://devtools/client/accessibility/constants.js");

/**
 * Fetch child accessibles for a given accessible object.
 * @param {Object} accessible front
 */
exports.fetchChildren =
  accessible =>
  ({ dispatch }) =>
    accessible
      .children()
      .then(response =>
        dispatch({ accessible, type: FETCH_CHILDREN, response })
      )
      .catch(error => dispatch({ accessible, type: FETCH_CHILDREN, error }));

exports.select =
  accessible =>
  ({ dispatch }) => {
    const accessibleWalkerFront = accessible.getParent();
    if (!accessibleWalkerFront) {
      dispatch({
        accessible,
        type: SELECT,
        error: new Error("AccessibleWalker front is not available."),
      });

      return Promise.reject();
    }

    return accessibleWalkerFront
      .getAncestry(accessible)
      .then(response => dispatch({ accessible, type: SELECT, response }))
      .catch(error => dispatch({ accessible, type: SELECT, error }));
  };

exports.highlight =
  accessible =>
  ({ dispatch }) => {
    const accessibleWalkerFront = accessible.getParent();
    if (!accessibleWalkerFront) {
      dispatch({
        accessible,
        type: SELECT,
        error: new Error("AccessibleWalker front is not available."),
      });

      return Promise.reject();
    }

    return accessibleWalkerFront
      .getAncestry(accessible)
      .then(response => dispatch({ accessible, type: HIGHLIGHT, response }))
      .catch(error => dispatch({ accessible, type: HIGHLIGHT, error }));
  };

exports.unhighlight =
  () =>
  ({ dispatch }) =>
    dispatch({ type: UNHIGHLIGHT });
