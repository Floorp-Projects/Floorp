/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { translateNodeFrontToGrip } = require("devtools/client/inspector/shared/utils");

// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("../types");

class FlexboxItem extends PureComponent {
  static get propTypes() {
    return {
      flexbox: PropTypes.shape(Types.flexbox).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onFlexboxCheckboxClick = this.onFlexboxCheckboxClick.bind(this);
    this.onFlexboxInspectIconClick = this.onFlexboxInspectIconClick.bind(this);
  }

  onFlexboxCheckboxClick(e) {
    // If the click was on the svg icon to select the node in the inspector, bail out.
    const originalTarget = e.nativeEvent && e.nativeEvent.explicitOriginalTarget;
    if (originalTarget && originalTarget.namespaceURI === "http://www.w3.org/2000/svg") {
      // We should be able to cancel the click event propagation after the following reps
      // issue is implemented : https://github.com/devtools-html/reps/issues/95 .
      e.preventDefault();
      return;
    }

    const {
      flexbox,
      onToggleFlexboxHighlighter,
    } = this.props;

    onToggleFlexboxHighlighter(flexbox.nodeFront);
  }

  onFlexboxInspectIconClick(nodeFront) {
    const { setSelectedNode } = this.props;
    setSelectedNode(nodeFront, { reason: "layout-panel" });
    nodeFront.scrollIntoView().catch(e => console.error(e));
  }

  render() {
    const {
      flexbox,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
    } = this.props;
    const {
      actorID,
      highlighted,
      nodeFront,
    } = flexbox;

    return dom.li(
      {},
      dom.label(
        {},
        dom.input(
          {
            type: "checkbox",
            value: actorID,
            checked: highlighted,
            onChange: this.onFlexboxCheckboxClick,
          }
        ),
        Rep(
          {
            defaultRep: ElementNode,
            mode: MODE.TINY,
            object: translateNodeFrontToGrip(nodeFront),
            onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
            onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(nodeFront),
            onInspectIconClick: () => this.onFlexboxInspectIconClick(nodeFront),
          }
        )
      )
    );
  }
}

module.exports = FlexboxItem;
