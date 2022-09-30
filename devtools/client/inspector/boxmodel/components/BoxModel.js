/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const BoxModelInfo = createFactory(
  require("resource://devtools/client/inspector/boxmodel/components/BoxModelInfo.js")
);
const BoxModelMain = createFactory(
  require("resource://devtools/client/inspector/boxmodel/components/BoxModelMain.js")
);
const BoxModelProperties = createFactory(
  require("resource://devtools/client/inspector/boxmodel/components/BoxModelProperties.js")
);

const Types = require("resource://devtools/client/inspector/boxmodel/types.js");

class BoxModel extends PureComponent {
  static get propTypes() {
    return {
      boxModel: PropTypes.shape(Types.boxModel).isRequired,
      dispatch: PropTypes.func.isRequired,
      onShowBoxModelEditor: PropTypes.func.isRequired,
      onShowRulePreviewTooltip: PropTypes.func.isRequired,
      onToggleGeometryEditor: PropTypes.func.isRequired,
      showBoxModelProperties: PropTypes.bool.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  onKeyDown(event) {
    const { target } = event;

    if (target == this.boxModelContainer) {
      this.boxModelMain.onKeyDown(event);
    }
  }

  render() {
    const {
      boxModel,
      dispatch,
      onShowBoxModelEditor,
      onShowRulePreviewTooltip,
      onToggleGeometryEditor,
      setSelectedNode,
      showBoxModelProperties,
    } = this.props;

    return dom.div(
      {
        className: "boxmodel-container",
        tabIndex: 0,
        ref: div => {
          this.boxModelContainer = div;
        },
        onKeyDown: this.onKeyDown,
      },
      BoxModelMain({
        boxModel,
        boxModelContainer: this.boxModelContainer,
        dispatch,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
        ref: boxModelMain => {
          this.boxModelMain = boxModelMain;
        },
      }),
      BoxModelInfo({
        boxModel,
        onToggleGeometryEditor,
      }),
      showBoxModelProperties
        ? BoxModelProperties({
            boxModel,
            dispatch,
            setSelectedNode,
          })
        : null
    );
  }
}

module.exports = BoxModel;
