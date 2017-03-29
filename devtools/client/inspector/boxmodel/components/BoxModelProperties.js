/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const { LocalizationHelper } = require("devtools/shared/l10n");

const ComputedProperty = createFactory(require("./ComputedProperty"));

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

module.exports = createClass({

  displayName: "BoxModelProperties",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      isOpen: true,
    };
  },

  onToggleExpander() {
    this.setState({
      isOpen: !this.state.isOpen,
    });
  },

  render() {
    let { boxModel } = this.props;
    let { layout } = boxModel;

    let layoutInfo = ["box-sizing", "display", "float",
                      "line-height", "position", "z-index"];

    const properties = layoutInfo.map(info => ComputedProperty({
      name: info,
      key: info,
      value: layout[info],
    }));

    return dom.div(
      {
        className: "boxmodel-properties",
      },
      dom.div(
        {
          className: "boxmodel-properties-header",
          onDoubleClick: this.onToggleExpander,
        },
        dom.span(
          {
            className: "boxmodel-properties-expander theme-twisty",
            open: this.state.isOpen,
            onClick: this.onToggleExpander,
          }
        ),
        BOXMODEL_L10N.getStr("boxmodel.propertiesLabel")
      ),
      dom.div(
        {
          className: "boxmodel-properties-wrapper",
          hidden: !this.state.isOpen,
          tabIndex: 0,
        },
        properties
      )
    );
  },

});
