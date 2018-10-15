/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createFactory,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const FlexContainer = createFactory(require("./FlexContainer"));
const FlexItemSelector = createFactory(require("./FlexItemSelector"));

const Types = require("../types");

class Header extends PureComponent {
  static get propTypes() {
    return {
      flexContainer: PropTypes.shape(Types.flexContainer).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      highlighted: PropTypes.bool.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onFlexboxCheckboxClick = this.onFlexboxCheckboxClick.bind(this);
  }

  onFlexboxCheckboxClick() {
    this.props.onToggleFlexboxHighlighter(this.props.flexContainer.nodeFront);
  }

  renderFlexboxHighlighterToggle() {
    // Don't show the flexbox highlighter toggle for the parent flex container of the
    // selected element.
    if (this.props.flexContainer.isFlexItemContainer) {
      return null;
    }

    return createElement(Fragment, null,
      dom.div({ className: "devtools-separator" }),
      dom.input({
        id: "flexbox-checkbox-toggle",
        className: "devtools-checkbox-toggle",
        checked: this.props.highlighted,
        onChange: this.onFlexboxCheckboxClick,
        type: "checkbox",
      })
    );
  }

  renderFlexContainer() {
    if (this.props.flexContainer.flexItemShown) {
      return null;
    }

    const {
      flexContainer,
      getSwatchColorPickerTooltip,
      onHideBoxModelHighlighter,
      onSetFlexboxOverlayColor,
      onShowBoxModelHighlighterForNode,
    } = this.props;

    return FlexContainer({
      flexContainer,
      getSwatchColorPickerTooltip,
      onHideBoxModelHighlighter,
      onSetFlexboxOverlayColor,
      onShowBoxModelHighlighterForNode,
    });
  }

  renderFlexItemSelector() {
    if (!this.props.flexContainer.flexItemShown) {
      return null;
    }

    const {
      flexContainer,
      setSelectedNode,
    } = this.props;
    const {
      flexItems,
      flexItemShown,
    } = flexContainer;
    const flexItem = flexItems.find(item => item.nodeFront.actorID === flexItemShown);

    if (!flexItem) {
      return null;
    }

    return FlexItemSelector({
      flexItem,
      flexItems,
      setSelectedNode,
    });
  }

  render() {
    const {
      flexContainer,
      setSelectedNode,
    } = this.props;
    const {
      flexItemShown,
      nodeFront,
    } = flexContainer;

    return (
      dom.div({ className: "flex-header devtools-monospace" },
        flexItemShown ?
          dom.button({
            className: "flex-header-button-prev devtools-button",
            onClick: () => setSelectedNode(nodeFront),
          })
          :
          null,
        dom.div(
          {
            className: "flex-header-content" +
                       (flexItemShown ? " flex-item-shown" : "")
          },
          this.renderFlexContainer(),
          this.renderFlexItemSelector()
        ),
        this.renderFlexboxHighlighterToggle()
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    highlighted: state.flexbox.highlighted,
  };
};

module.exports = connect(mapStateToProps)(Header);
