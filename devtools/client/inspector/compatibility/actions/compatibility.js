/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { COMPATIBILITY_UPDATE_SELECTED_NODE } = require("./index");

function updateSelectedNode(nodeFront) {
  return async ({ dispatch }) => {
    const pageStyle = nodeFront.inspectorFront.pageStyle;
    const styles = await pageStyle.getApplied(nodeFront, { skipPseudo: false });
    const declarationBlocks = styles
      .map(({ rule }) => rule.declarations.filter(d => !d.commentOffsets))
      .filter(declarations => declarations.length);

    dispatch({ type: COMPATIBILITY_UPDATE_SELECTED_NODE, declarationBlocks });
  };
}

module.exports = {
  updateSelectedNode,
};
