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

loader.lazyGetter(this, "FlexContainerProperties", function() {
  return createFactory(require("./FlexContainerProperties"));
});
loader.lazyGetter(this, "FlexItemList", function() {
  return createFactory(require("./FlexItemList"));
});
loader.lazyGetter(this, "FlexItemSizingOutline", function() {
  return createFactory(require("./FlexItemSizingOutline"));
});
loader.lazyGetter(this, "FlexItemSizingProperties", function() {
  return createFactory(require("./FlexItemSizingProperties"));
});
loader.lazyGetter(this, "Header", function() {
  return createFactory(require("./Header"));
});

const Types = require("../types");

class Flexbox extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      flexContainer: PropTypes.shape(Types.flexContainer).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  renderFlexContainerProperties() {
    // Don't show the flex container properties for the parent flex container of the
    // selected element.
    if (this.props.flexContainer.isFlexItemContainer) {
      return null;
    }

    return FlexContainerProperties({
      properties: this.props.flexContainer.properties,
    });
  }

  renderFlexItemList() {
    const { setSelectedNode } = this.props;
    const { flexItems } = this.props.flexContainer;

    return FlexItemList({
      flexItems,
      setSelectedNode,
    });
  }

  renderFlexItemSizing() {
    const {
      color,
    } = this.props.flexbox;
    const {
      flexItems,
      flexItemShown,
      properties,
    } = this.props.flexContainer;

    const flexItem = flexItems.find(item => item.nodeFront.actorID === flexItemShown);
    if (!flexItem) {
      return null;
    }

    return createElement(Fragment, null,
      FlexItemSizingOutline({
        color,
        flexDirection: properties["flex-direction"],
        flexItem,
      }),
      FlexItemSizingProperties({
        flexDirection: properties["flex-direction"],
        flexItem,
      })
    );
  }

  render() {
    const {
      flexContainer,
      getSwatchColorPickerTooltip,
      onHideBoxModelHighlighter,
      onSetFlexboxOverlayColor,
      onShowBoxModelHighlighterForNode,
      onToggleFlexboxHighlighter,
      setSelectedNode,
    } = this.props;

    if (!flexContainer.actorID) {
      return (
        dom.div({ className: "devtools-sidepanel-no-result" },
          getStr("flexbox.noFlexboxeOnThisPage")
        )
      );
    }

    const {
      flexItems,
      flexItemShown,
    } = flexContainer;

    return (
      dom.div({ id: "layout-flexbox-container" },
        Header({
          flexContainer,
          getSwatchColorPickerTooltip,
          onHideBoxModelHighlighter,
          onSetFlexboxOverlayColor,
          onShowBoxModelHighlighterForNode,
          onToggleFlexboxHighlighter,
          setSelectedNode,
        }),
        !flexItemShown && flexItems.length > 0 ? this.renderFlexItemList() : null,
        flexItemShown ? this.renderFlexItemSizing() : null,
        this.renderFlexContainerProperties()
      )
    );
  }
}

module.exports = Flexbox;
