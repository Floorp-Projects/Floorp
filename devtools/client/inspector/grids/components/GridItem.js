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

loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

loader.lazyRequireGetter(
  this,
  "translateNodeFrontToGrip",
  "devtools/client/inspector/shared/utils",
  true
);

const Types = require("../types");

class GridItem extends PureComponent {
  static get propTypes() {
    return {
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grid: PropTypes.shape(Types.grid).isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.colorValueEl = createRef();
    this.swatchEl = createRef();

    this.onGridCheckboxClick = this.onGridCheckboxClick.bind(this);
    this.onGridInspectIconClick = this.onGridInspectIconClick.bind(this);
    this.setGridColor = this.setGridColor.bind(this);
  }

  componentDidMount() {
    const tooltip = this.props.getSwatchColorPickerTooltip();

    let previousColor;
    tooltip.addSwatch(this.swatchEl.current, {
      onCommit: this.setGridColor,
      onPreview: this.setGridColor,
      onRevert: () => {
        this.props.onSetGridOverlayColor(
          this.props.grid.nodeFront,
          previousColor
        );
      },
      onShow: () => {
        previousColor = this.props.grid.color;
      },
    });
  }

  componentWillUnmount() {
    const tooltip = this.props.getSwatchColorPickerTooltip();
    tooltip.removeSwatch(this.swatchEl.current);
  }

  setGridColor() {
    const color = this.colorValueEl.current.textContent;
    this.props.onSetGridOverlayColor(this.props.grid.nodeFront, color);
  }

  onGridCheckboxClick() {
    const { grid, onToggleGridHighlighter } = this.props;
    onToggleGridHighlighter(grid.nodeFront);
  }

  onGridInspectIconClick(nodeFront) {
    const { setSelectedNode } = this.props;
    setSelectedNode(nodeFront, { reason: "layout-panel" });
    nodeFront.scrollIntoView().catch(e => console.error(e));
  }

  renderSubgrids() {
    const { grid, grids } = this.props;

    if (!grid.subgrids.length) {
      return null;
    }

    const subgrids = grids.filter(g => grid.subgrids.includes(g.id));

    return dom.ul(
      {},
      subgrids.map(g => {
        return createElement(GridItem, {
          key: g.id,
          getSwatchColorPickerTooltip: this.props.getSwatchColorPickerTooltip,
          grid: g,
          grids,
          onHideBoxModelHighlighter: this.props.onHideBoxModelHighlighter,
          onSetGridOverlayColor: this.props.onSetGridOverlayColor,
          onShowBoxModelHighlighterForNode: this.props
            .onShowBoxModelHighlighterForNode,
          onToggleGridHighlighter: this.props.onToggleGridHighlighter,
          setSelectedNode: this.props.setSelectedNode,
        });
      })
    );
  }

  render() {
    const {
      grid,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
    } = this.props;

    return createElement(
      Fragment,
      null,
      dom.li(
        {},
        dom.label(
          {},
          dom.input({
            checked: grid.highlighted,
            disabled: grid.disabled,
            type: "checkbox",
            value: grid.id,
            onChange: this.onGridCheckboxClick,
          }),
          Rep({
            defaultRep: Rep.ElementNode,
            mode: MODE.TINY,
            object: translateNodeFrontToGrip(grid.nodeFront),
            onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
            onDOMNodeMouseOver: () =>
              onShowBoxModelHighlighterForNode(grid.nodeFront),
            onInspectIconClick: (_, e) => {
              // Stoping click propagation to avoid firing onGridCheckboxClick()
              e.stopPropagation();
              this.onGridInspectIconClick(grid.nodeFront);
            },
          })
        ),
        dom.div({
          className: "layout-color-swatch",
          ref: this.swatchEl,
          style: {
            backgroundColor: grid.color,
          },
          title: grid.color,
        }),
        // The SwatchColorPicker relies on the nextSibling of the swatch element to apply
        // the selected color. This is why we use a span in display: none for now.
        // Ideally we should modify the SwatchColorPickerTooltip to bypass this
        // requirement. See https://bugzilla.mozilla.org/show_bug.cgi?id=1341578
        dom.span(
          {
            className: "layout-color-value",
            ref: this.colorValueEl,
          },
          grid.color
        )
      ),
      this.renderSubgrids()
    );
  }
}

module.exports = GridItem;
