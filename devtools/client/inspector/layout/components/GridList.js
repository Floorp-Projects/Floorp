/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const { getStr } = require("../utils/l10n");

module.exports = createClass({

  displayName: "GridList",

  propTypes: {
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
    onToggleGridHighlighter: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  onGridCheckboxClick({ target }) {
    let {
      grids,
      onToggleGridHighlighter,
    } = this.props;

    onToggleGridHighlighter(grids[target.value].nodeFront);
  },

  render() {
    let {
      grids,
    } = this.props;

    return dom.div(
      {
        className: "layout-grid-list-container",
      },
      dom.span(
        {},
        getStr("layout.overlayMultipleGrids")
      ),
      dom.ul(
        {
          id: "layout-grid-list",
        },
        grids.map(grid => {
          let { nodeFront } = grid;
          let { displayName, attributes } = nodeFront;

          let gridName = displayName;

          let idIndex = attributes.findIndex(({ name }) => name === "id");
          if (idIndex > -1 && attributes[idIndex].value) {
            gridName += "#" + attributes[idIndex].value;
          }

          let classIndex = attributes.findIndex(({name}) => name === "class");
          if (classIndex > -1 && attributes[classIndex].value) {
            gridName += "." + attributes[classIndex].value.split(" ").join(".");
          }

          return dom.li(
            {
              key: grid.id,
            },
            dom.label(
              {},
              dom.input(
                {
                  type: "checkbox",
                  value: grid.id,
                  checked: grid.highlighted,
                  onChange: this.onGridCheckboxClick,
                }
              ),
              gridName
            )
          );
        })
      )
    );
  },

});

