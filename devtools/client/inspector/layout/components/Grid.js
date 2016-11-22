/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const { getStr } = require("../utils/l10n");

module.exports = createClass({

  displayName: "Grid",

  propTypes: {
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let {
      grids,
    } = this.props;

    return dom.div(
      {
        id: "layout-grid-container",
      },
      !grids.length ?
        dom.div(
          {
            className: "layout-no-grids"
          },
          getStr("layout.noGrids")
        ) : null
    );
  },

});
