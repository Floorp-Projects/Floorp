/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimationTarget = createFactory(require("./AnimationTarget"));

class AnimationItem extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      animation,
      emitEventForTest,
      getNodeFromActor,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      setSelectedNode,
    } = this.props;

    return dom.li(
      {
        className: "animation-item"
      },
      AnimationTarget(
        {
          animation,
          emitEventForTest,
          getNodeFromActor,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          setSelectedNode,
        }
      )
    );
  }
}

module.exports = AnimationItem;
