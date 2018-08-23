/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

loader.lazyGetter(this, "FlexContainerList", function() {
  return createFactory(require("./FlexContainerList"));
});
loader.lazyGetter(this, "FlexContainerProperties", function() {
  return createFactory(require("./FlexContainerProperties"));
});

const Types = require("../types");

class Flexbox extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      flexbox,
      getSwatchColorPickerTooltip,
      onHideBoxModelHighlighter,
      onSetFlexboxOverlayColor,
      onShowBoxModelHighlighterForNode,
      onToggleFlexboxHighlighter,
      setSelectedNode,
    } = this.props;

    if (!flexbox.actorID) {
      return (
        dom.div({ className: "devtools-sidepanel-no-result" },
          getStr("flexbox.noFlexboxeOnThisPage")
        )
      );
    }

    return (
      dom.div({ id: "layout-flexbox-container" },
        dom.div({ className: "flexbox-content" },
          FlexContainerList({
            flexbox,
            getSwatchColorPickerTooltip,
            onHideBoxModelHighlighter,
            onSetFlexboxOverlayColor,
            onShowBoxModelHighlighterForNode,
            onToggleFlexboxHighlighter,
            setSelectedNode,
          })
        ),
        FlexContainerProperties({
          properties: flexbox.properties,
        })
      )
    );
  }
}

module.exports = Flexbox;
