/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("../utils/l10n");
const { DOM: dom, createClass } = require("devtools/client/shared/vendor/react");

const Grid = createClass({

  displayName: "Grid",

  render() {
    return dom.div(
      {
        id: "layoutview-grid-container",
      },
      dom.div(
        {
          className: "layoutview-no-grids"
        },
        getStr("layout.noGrids")
      )
    );
  },

});

module.exports = Grid;
