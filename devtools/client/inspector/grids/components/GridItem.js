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

class GridItem extends PureComponent {
  static get propTypes() {
    return {
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grid: PropTypes.shape(Types.grid).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.setGridColor = this.setGridColor.bind(this);
    this.onGridCheckboxClick = this.onGridCheckboxClick.bind(this);
    this.onGridInspectIconClick = this.onGridInspectIconClick.bind(this);
  }

  componentDidMount() {
    const swatchEl = findDOMNode(this).querySelector(".grid-color-swatch");
    const tooltip = this.props.getSwatchColorPickerTooltip();

    let previousColor;
    tooltip.addSwatch(swatchEl, {
      onCommit: this.setGridColor,
      onPreview: this.setGridColor,
      onRevert: () => {
        this.props.onSetGridOverlayColor(this.props.grid.nodeFront, previousColor);
      },
      onShow: () => {
        previousColor = this.props.grid.color;
      },
    });
  }

  componentWillUnmount() {
    const swatchEl = findDOMNode(this).querySelector(".grid-color-swatch");
    const tooltip = this.props.getSwatchColorPickerTooltip();
    tooltip.removeSwatch(swatchEl);
  }

  setGridColor() {
    const color = findDOMNode(this).querySelector(".grid-color-value").textContent;
    this.props.onSetGridOverlayColor(this.props.grid.nodeFront, color);
  }

  onGridCheckboxClick(e) {
    // If the click was on the svg icon to select the node in the inspector, bail out.
    const originalTarget = e.nativeEvent && e.nativeEvent.explicitOriginalTarget;
    if (originalTarget && originalTarget.namespaceURI === "http://www.w3.org/2000/svg") {
      // We should be able to cancel the click event propagation after the following reps
      // issue is implemented : https://github.com/devtools-html/reps/issues/95 .
      e.preventDefault();
      return;
    }

    const {
      grid,
      onToggleGridHighlighter,
    } = this.props;

    onToggleGridHighlighter(grid.nodeFront);
  }

  onGridInspectIconClick(nodeFront) {
    const { setSelectedNode } = this.props;
    setSelectedNode(nodeFront, { reason: "layout-panel" });
    nodeFront.scrollIntoView().catch(e => console.error(e));
  }

  render() {
    const {
      grid,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
    } = this.props;
    const { nodeFront } = grid;

    return dom.li(
      {},
      dom.label(
        {},
        dom.input(
          {
            checked: grid.highlighted,
            type: "checkbox",
            value: grid.id,
            onChange: this.onGridCheckboxClick,
          }
        ),
        Rep(
          {
            defaultRep: ElementNode,
            mode: MODE.TINY,
            object: translateNodeFrontToGrip(nodeFront),
            onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
            onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(nodeFront),
            onInspectIconClick: () => this.onGridInspectIconClick(nodeFront),
          }
        )
      ),
      dom.div(
        {
          className: "grid-color-swatch",
          style: {
            backgroundColor: grid.color,
          },
          title: grid.color,
        }
      ),
      // The SwatchColorPicker relies on the nextSibling of the swatch element to apply
      // the selected color. This is why we use a span in display: none for now.
      // Ideally we should modify the SwatchColorPickerTooltip to bypass this requirement.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1341578
      dom.span(
        {
          className: "grid-color-value"
        },
        grid.color
      )
    );
  }
}

module.exports = GridItem;
