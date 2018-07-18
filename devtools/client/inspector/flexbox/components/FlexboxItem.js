/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { translateNodeFrontToGrip } = require("devtools/client/inspector/shared/utils");

// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("../types");

class FlexboxItem extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.setFlexboxColor = this.setFlexboxColor.bind(this);
    this.onFlexboxCheckboxClick = this.onFlexboxCheckboxClick.bind(this);
    this.onFlexboxInspectIconClick = this.onFlexboxInspectIconClick.bind(this);
  }

  componentDidMount() {
    const {
      flexbox,
      getSwatchColorPickerTooltip,
      onSetFlexboxOverlayColor,
    } = this.props;

    const swatchEl = findDOMNode(this).querySelector(".flexbox-color-swatch");
    const tooltip = getSwatchColorPickerTooltip();

    let previousColor;
    tooltip.addSwatch(swatchEl, {
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
    const swatchEl = findDOMNode(this).querySelector(".flexbox-color-swatch");
    const tooltip = this.props.getSwatchColorPickerTooltip();
    tooltip.removeSwatch(swatchEl);
  }

  setFlexboxColor() {
    const color = findDOMNode(this).querySelector(".flexbox-color-value").textContent;
    this.props.onSetFlexboxOverlayColor(color);
  }

  onFlexboxCheckboxClick(e) {
    // If the click was on the svg icon to select the node in the inspector, bail out.
    const originalTarget = e.nativeEvent && e.nativeEvent.explicitOriginalTarget;
    if (originalTarget && originalTarget.namespaceURI === "http://www.w3.org/2000/svg") {
      // We should be able to cancel the click event propagation after the following reps
      // issue is implemented : https://github.com/devtools-html/reps/issues/95 .
      e.preventDefault();
      return;
    }

    const {
      flexbox,
      onToggleFlexboxHighlighter,
    } = this.props;

    onToggleFlexboxHighlighter(flexbox.nodeFront);
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
      actorID,
      color,
      highlighted,
      nodeFront,
    } = flexbox;

    return dom.li(
      {},
      dom.label(
        {},
        dom.input(
          {
            type: "checkbox",
            value: actorID,
            checked: highlighted,
            onChange: this.onFlexboxCheckboxClick,
          }
        ),
        Rep(
          {
            defaultRep: ElementNode,
            mode: MODE.TINY,
            object: translateNodeFrontToGrip(nodeFront),
            onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
            onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(nodeFront),
            onInspectIconClick: () => this.onFlexboxInspectIconClick(nodeFront),
          }
        )
      ),
      dom.div(
        {
          className: "flexbox-color-swatch",
          style: {
            backgroundColor: color,
          },
          title: color,
        }
      ),
      // The SwatchColorPicker relies on the nextSibling of the swatch element to apply
      // the selected color. This is why we use a span in display: none for now.
      // Ideally we should modify the SwatchColorPickerTooltip to bypass this requirement.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1341578
      dom.span({ className: "flexbox-color-value" }, color)
    );
  }
}

module.exports = FlexboxItem;
