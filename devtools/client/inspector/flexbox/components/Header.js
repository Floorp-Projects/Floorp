/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FlexContainer = createFactory(require("./FlexContainer"));
const FlexItemSelector = createFactory(require("./FlexItemSelector"));

const Types = require("../types");

class Header extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
      onToggleFlexItemShown: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onFlexboxCheckboxClick = this.onFlexboxCheckboxClick.bind(this);
  }

  onFlexboxCheckboxClick() {
    const {
      flexbox,
      onToggleFlexboxHighlighter,
    } = this.props;

    onToggleFlexboxHighlighter(flexbox.nodeFront);
  }

  renderFlexContainer() {
    if (this.props.flexbox.flexItemShown) {
      return null;
    }

    const {
      flexbox,
      getSwatchColorPickerTooltip,
      onHideBoxModelHighlighter,
      onSetFlexboxOverlayColor,
      onShowBoxModelHighlighterForNode,
      setSelectedNode,
    } = this.props;

    return FlexContainer({
      flexbox,
      getSwatchColorPickerTooltip,
      onHideBoxModelHighlighter,
      onSetFlexboxOverlayColor,
      onShowBoxModelHighlighterForNode,
      setSelectedNode,
    });
  }

  renderFlexItemSelector() {
    if (!this.props.flexbox.flexItemShown) {
      return null;
    }

    const {
      flexbox,
      onToggleFlexItemShown,
    } = this.props;
    const {
      flexItems,
      flexItemShown,
    } = flexbox;

    return FlexItemSelector({
      flexItem: flexItems.find(item => item.nodeFront.actorID === flexItemShown),
      flexItems,
      onToggleFlexItemShown,
    });
  }

  render() {
    const { highlighted } = this.props.flexbox;

    return (
      dom.div({ className: "flex-header devtools-monospace" },
        dom.div({ className: "flex-header-content" },
          this.renderFlexContainer(),
          this.renderFlexItemSelector()
        ),
        dom.div({ className: "devtools-separator" }),
        dom.input({
          className: "devtools-checkbox-toggle",
          checked: highlighted,
          onChange: this.onFlexboxCheckboxClick,
          type: "checkbox",
        })
      )
    );
  }
}

module.exports = Header;
