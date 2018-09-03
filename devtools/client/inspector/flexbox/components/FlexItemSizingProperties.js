/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const Types = require("../types");

class FlexItemSizingProperties extends PureComponent {
  static get propTypes() {
    return {
      flexDirection: PropTypes.string.isRequired,
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
    };
  }

  render() {
    const {
      flexDirection,
      flexItem,
    } = this.props;
    const {
      flexItemSizing,
      properties,
    } = flexItem;
    const dimension = flexDirection.startsWith("row") ? "width" : "height";
    const contentStr = dimension === "width" ?
      getStr("flexbox.contentWidth") : getStr("flexbox.contentHeight");
    const finalStr = dimension === "width" ?
      getStr("flexbox.finalWidth") : getStr("flexbox.finalHeight");

    return (
      dom.ol(
        {
          id: "flex-item-sizing-properties",
          className: "flex-item-list",
        },
        dom.li({},
          dom.span({}, "flex-basis: "),
          properties["flex-basis"]
        ),
        dom.li({},
          dom.span({}, "flex-grow: "),
          properties["flex-grow"]
        ),
        dom.li({},
          dom.span({}, "flex-shrink: "),
          properties["flex-shrink"]
        ),
        dom.li({},
          dom.span({}, `${contentStr} `),
          `${parseFloat(flexItemSizing.mainBaseSize.toPrecision(6))}px`
        ),
        dom.li({},
          dom.span({}, `Min-${dimension}: `),
          properties["min-" + dimension]
        ),
        dom.li({},
          dom.span({}, `Max-${dimension}: `),
          properties["max-" + dimension]
        ),
        dom.li({},
          dom.span({}, `${finalStr} `),
          `${parseFloat(properties[dimension].toPrecision(6))}px`
        )
      )
    );
  }
}

module.exports = FlexItemSizingProperties;
