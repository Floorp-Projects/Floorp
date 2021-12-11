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

loader.lazyGetter(this, "FlexItemList", function() {
  return createFactory(
    require("devtools/client/inspector/flexbox/components/FlexItemList")
  );
});
loader.lazyGetter(this, "FlexItemSizingOutline", function() {
  return createFactory(
    require("devtools/client/inspector/flexbox/components/FlexItemSizingOutline")
  );
});
loader.lazyGetter(this, "FlexItemSizingProperties", function() {
  return createFactory(
    require("devtools/client/inspector/flexbox/components/FlexItemSizingProperties")
  );
});
loader.lazyGetter(this, "Header", function() {
  return createFactory(
    require("devtools/client/inspector/flexbox/components/Header")
  );
});

const Types = require("devtools/client/inspector/flexbox/types");

class Flexbox extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      flexContainer: PropTypes.shape(Types.flexContainer).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      scrollToTop: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  renderFlexItemList() {
    const { dispatch, scrollToTop, setSelectedNode } = this.props;
    const { flexItems } = this.props.flexContainer;

    return FlexItemList({
      dispatch,
      flexItems,
      scrollToTop,
      setSelectedNode,
    });
  }

  renderFlexItemSizing() {
    const { flexItems, flexItemShown, properties } = this.props.flexContainer;

    const flexItem = flexItems.find(
      item => item.nodeFront.actorID === flexItemShown
    );
    if (!flexItem) {
      return null;
    }

    return createElement(
      Fragment,
      null,
      FlexItemSizingOutline({
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
      dispatch,
      flexbox,
      flexContainer,
      getSwatchColorPickerTooltip,
      onSetFlexboxOverlayColor,
      setSelectedNode,
    } = this.props;

    if (!flexContainer.actorID) {
      return dom.div(
        { className: "devtools-sidepanel-no-result" },
        getStr("flexbox.noFlexboxeOnThisPage")
      );
    }

    const { flexItemShown } = flexContainer;
    const { color, highlighted } = flexbox;

    return dom.div(
      { className: "layout-flexbox-wrapper" },
      Header({
        color,
        dispatch,
        flexContainer,
        getSwatchColorPickerTooltip,
        highlighted,
        onSetFlexboxOverlayColor,
        setSelectedNode,
      }),
      !flexItemShown ? this.renderFlexItemList() : null,
      flexItemShown ? this.renderFlexItemSizing() : null
    );
  }
}

module.exports = Flexbox;
