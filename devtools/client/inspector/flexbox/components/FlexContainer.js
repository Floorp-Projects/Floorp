/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createRef,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { translateNodeFrontToGrip } = require("devtools/client/inspector/shared/utils");

const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("../types");

class FlexContainer extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.colorValueEl = createRef();
    this.swatchEl = createRef();

    this.onFlexboxInspectIconClick = this.onFlexboxInspectIconClick.bind(this);
    this.setFlexboxColor = this.setFlexboxColor.bind(this);
  }

  componentDidMount() {
    const {
      flexbox,
      getSwatchColorPickerTooltip,
      onSetFlexboxOverlayColor,
    } = this.props;

    const tooltip = getSwatchColorPickerTooltip();

    let previousColor;
    tooltip.addSwatch(this.swatchEl.current, {
      onCommit: this.setFlexboxColor,
      onPreview: this.setFlexboxColor,
      onRevert: () => {
        onSetFlexboxOverlayColor(previousColor);
      },
      onShow: () => {
        previousColor = flexbox.color;
      },
    });
  }

  componentWillUnMount() {
    const tooltip = this.props.getSwatchColorPickerTooltip();
    tooltip.removeSwatch(this.swatchEl.current);
  }

  setFlexboxColor() {
    const color = this.colorValueEl.current.textContent;
    this.props.onSetFlexboxOverlayColor(color);
  }

  onFlexboxInspectIconClick(nodeFront) {
    const { setSelectedNode } = this.props;
    setSelectedNode(nodeFront, { reason: "layout-panel" });
    nodeFront.scrollIntoView().catch(e => console.error(e));
  }

  render() {
    const {
      flexbox,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
    } = this.props;
    const {
      color,
      nodeFront,
    } = flexbox;

    return createElement(Fragment, null,
      Rep({
        defaultRep: ElementNode,
        mode: MODE.TINY,
        object: translateNodeFrontToGrip(nodeFront),
        onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
        onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(nodeFront),
        onInspectIconClick: () => this.onFlexboxInspectIconClick(nodeFront),
      }),
      dom.div({
        className: "layout-color-swatch",
        ref: this.swatchEl,
        style: {
          backgroundColor: color,
        },
        title: color,
      }),
      // The SwatchColorPicker relies on the nextSibling of the swatch element to
      // apply the selected color. This is why we use a span in display: none for
      // now. Ideally we should modify the SwatchColorPickerTooltip to bypass this
      // requirement. See https://bugzilla.mozilla.org/show_bug.cgi?id=1341578
      dom.span(
        {
          className: "layout-color-value",
          ref: this.colorValueEl,
        },
        color
      )
    );
  }
}

module.exports = FlexContainer;
