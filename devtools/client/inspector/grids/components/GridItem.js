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
  getStr,
} = require("resource://devtools/client/inspector/layout/utils/l10n.js");

loader.lazyGetter(this, "Rep", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .REPS.Rep;
});
loader.lazyGetter(this, "MODE", function () {
  return require("resource://devtools/client/shared/components/reps/index.js")
    .MODE;
});

loader.lazyRequireGetter(
  this,
  "translateNodeFrontToGrip",
  "resource://devtools/client/inspector/shared/utils.js",
  true
);

const Types = require("resource://devtools/client/inspector/grids/types.js");

const {
  highlightNode,
  unhighlightNode,
} = require("resource://devtools/client/inspector/boxmodel/actions/box-model-highlighter.js");

class GridItem extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grid: PropTypes.shape(Types.grid).isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

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
    const color = this.swatchEl.current.dataset.color;
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
          dispatch: this.props.dispatch,
          getSwatchColorPickerTooltip: this.props.getSwatchColorPickerTooltip,
          grid: g,
          grids,
          onSetGridOverlayColor: this.props.onSetGridOverlayColor,
          onToggleGridHighlighter: this.props.onToggleGridHighlighter,
          setSelectedNode: this.props.setSelectedNode,
        });
      })
    );
  }

  render() {
    const { dispatch, grid } = this.props;

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
            title: getStr("layout.toggleGridHighlighter"),
          }),
          Rep({
            defaultRep: Rep.ElementNode,
            mode: MODE.TINY,
            object: translateNodeFrontToGrip(grid.nodeFront),
            onDOMNodeMouseOut: () => dispatch(unhighlightNode()),
            onDOMNodeMouseOver: () => dispatch(highlightNode(grid.nodeFront)),
            onInspectIconClick: (_, e) => {
              // Stoping click propagation to avoid firing onGridCheckboxClick()
              e.stopPropagation();
              this.onGridInspectIconClick(grid.nodeFront);
            },
          })
        ),
        dom.button({
          className: "layout-color-swatch",
          "data-color": grid.color,
          ref: this.swatchEl,
          style: {
            backgroundColor: grid.color,
          },
          title: getFormatStr("layout.colorSwatch.tooltip", grid.color),
        })
      ),
      this.renderSubgrids()
    );
  }
}

module.exports = GridItem;
