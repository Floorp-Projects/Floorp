/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const FlexboxItem = createFactory(require("./FlexboxItem"));

const Types = require("../types");

class Flexbox extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      flexbox,
      setSelectedNode,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      onToggleFlexboxHighlighter,
    } = this.props;

    return flexbox.actorID ?
      dom.div({ id: "layout-flexbox-container" },
        dom.div({ className: "flexbox-content" },
          dom.div({ className: "flexbox-container" },
            dom.span({}, getStr("flexbox.overlayFlexbox")),
            dom.ul(
              {
                id: "flexbox-list",
                className: "devtools-monospace",
              },
              FlexboxItem({
                key: flexbox.id,
                flexbox,
                setSelectedNode,
                onHideBoxModelHighlighter,
                onShowBoxModelHighlighterForNode,
                onToggleFlexboxHighlighter,
              })
            )
          )
        )
      )
      :
      dom.div({ className: "devtools-sidepanel-no-result" },
        getStr("flexbox.noFlexboxeOnThisPage")
      );
  }
}

module.exports = Flexbox;
