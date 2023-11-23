/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createRef,
  Fragment,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getFormatStr,
} = require("resource://devtools/client/inspector/layout/utils/l10n.js");

loader.lazyRequireGetter(
  this,
  "getNodeRep",
  "resource://devtools/client/inspector/shared/node-reps.js"
);

const Types = require("resource://devtools/client/inspector/flexbox/types.js");

const {
  highlightNode,
  unhighlightNode,
} = require("resource://devtools/client/inspector/boxmodel/actions/box-model-highlighter.js");

class FlexContainer extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      color: PropTypes.string.isRequired,
      flexContainer: PropTypes.shape(Types.flexContainer).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.swatchEl = createRef();

    this.setFlexboxColor = this.setFlexboxColor.bind(this);
  }

  componentDidMount() {
    const tooltip = this.props.getSwatchColorPickerTooltip();

    let previousColor;
    tooltip.addSwatch(this.swatchEl.current, {
      onCommit: this.setFlexboxColor,
      onPreview: this.setFlexboxColor,
      onRevert: () => {
        this.props.onSetFlexboxOverlayColor(previousColor);
      },
      onShow: () => {
        previousColor = this.props.color;
      },
    });
  }

  componentWillUnmount() {
    const tooltip = this.props.getSwatchColorPickerTooltip();
    tooltip.removeSwatch(this.swatchEl.current);
  }

  setFlexboxColor() {
    const color = this.swatchEl.current.dataset.color;
    this.props.onSetFlexboxOverlayColor(color);
  }

  render() {
    const { color, flexContainer, dispatch } = this.props;
    const { nodeFront, properties } = flexContainer;

    return createElement(
      Fragment,
      null,
      dom.div(
        {
          className: "flex-header-container-label",
        },
        getNodeRep(nodeFront, {
          onDOMNodeMouseOut: () => dispatch(unhighlightNode()),
          onDOMNodeMouseOver: () => dispatch(highlightNode(nodeFront)),
        }),
        dom.button({
          className: "layout-color-swatch",
          "data-color": color,
          ref: this.swatchEl,
          style: {
            backgroundColor: color,
          },
          title: getFormatStr("layout.colorSwatch.tooltip", color),
        })
      ),
      dom.div(
        { className: "flex-header-container-properties" },
        dom.div(
          {
            className: "inspector-badge",
            role: "figure",
            title: `flex-direction: ${properties["flex-direction"]}`,
          },
          properties["flex-direction"]
        ),
        dom.div(
          {
            className: "inspector-badge",
            role: "figure",
            title: `flex-wrap: ${properties["flex-wrap"]}`,
          },
          properties["flex-wrap"]
        )
      )
    );
  }
}

module.exports = FlexContainer;
