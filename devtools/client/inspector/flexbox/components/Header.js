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
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const FlexContainer = createFactory(
  require("devtools/client/inspector/flexbox/components/FlexContainer")
);
const FlexItemSelector = createFactory(
  require("devtools/client/inspector/flexbox/components/FlexItemSelector")
);

const Types = require("devtools/client/inspector/flexbox/types");

class Header extends PureComponent {
  static get propTypes() {
    return {
      color: PropTypes.string.isRequired,
      dispatch: PropTypes.func.isRequired,
      flexContainer: PropTypes.shape(Types.flexContainer).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      highlighted: PropTypes.bool.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
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

    return createElement(
      Fragment,
      null,
      dom.div({ className: "devtools-separator" }),
      dom.input({
        id: "flexbox-checkbox-toggle",
        className: "devtools-checkbox-toggle",
        checked: this.props.highlighted,
        onChange: this.onFlexboxCheckboxClick,
        title: getStr("flexbox.togglesFlexboxHighlighter2"),
        type: "checkbox",
      })
    );
  }

  renderFlexContainer() {
    if (this.props.flexContainer.flexItemShown) {
      return null;
    }

    const {
      color,
      dispatch,
      flexContainer,
      getSwatchColorPickerTooltip,
      onSetFlexboxOverlayColor,
    } = this.props;

    return FlexContainer({
      color,
      dispatch,
      flexContainer,
      getSwatchColorPickerTooltip,
      onSetFlexboxOverlayColor,
    });
  }

  renderFlexItemSelector() {
    if (!this.props.flexContainer.flexItemShown) {
      return null;
    }

    const { flexContainer, setSelectedNode } = this.props;
    const { flexItems, flexItemShown } = flexContainer;
    const flexItem = flexItems.find(
      item => item.nodeFront.actorID === flexItemShown
    );

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
    const { flexContainer, setSelectedNode } = this.props;
    const { flexItemShown, nodeFront } = flexContainer;

    return dom.div(
      { className: "flex-header devtools-monospace" },
      flexItemShown
        ? dom.button({
            className: "flex-header-button-prev devtools-button",
            "aria-label": getStr("flexbox.backButtonLabel"),
            onClick: e => {
              e.stopPropagation();
              setSelectedNode(nodeFront);
            },
          })
        : null,
      dom.div(
        {
          className:
            "flex-header-content" + (flexItemShown ? " flex-item-shown" : ""),
        },
        this.renderFlexContainer(),
        this.renderFlexItemSelector()
      ),
      this.renderFlexboxHighlighterToggle()
    );
  }
}

module.exports = Header;
