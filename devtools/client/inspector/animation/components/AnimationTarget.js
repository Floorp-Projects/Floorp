/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  translateNodeFrontToGrip,
} = require("devtools/client/inspector/shared/utils");

const { REPS, MODE } = require("devtools/client/shared/components/reps/index");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const {
  getInspectorStr,
} = require("devtools/client/inspector/animation/utils/l10n");

const {
  highlightNode,
  unhighlightNode,
} = require("devtools/client/inspector/boxmodel/actions/box-model-highlighter");

class AnimationTarget extends Component {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      highlightedNode: PropTypes.string.isRequired,
      setHighlightedNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      nodeFront: null,
    };
  }

  componentWillMount() {
    this.updateNodeFront(this.props.animation);
  }

  componentWillReceiveProps(nextProps) {
    if (this.props.animation.actorID !== nextProps.animation.actorID) {
      this.updateNodeFront(nextProps.animation);
    }
  }

  shouldComponentUpdate(nextProps, nextState) {
    return (
      this.state.nodeFront !== nextState.nodeFront ||
      this.props.highlightedNode !== nextState.highlightedNode
    );
  }

  async updateNodeFront(animation) {
    const { getNodeFromActor } = this.props;

    // Try and get it from the playerFront directly.
    let nodeFront = animation.animationTargetNodeFront;

    // Next, get it from the walkerActor if it wasn't found.
    if (!nodeFront) {
      try {
        nodeFront = await getNodeFromActor(animation.actorID);
      } catch (e) {
        // If an error occured while getting the nodeFront and if it can't be
        // attributed to the panel having been destroyed in the meantime, this
        // error needs to be logged and render needs to stop.
        console.error(e);
        this.setState({ nodeFront: null });
        return;
      }
    }

    this.setState({ nodeFront });
  }

  async ensureNodeFront() {
    if (!this.state.nodeFront.actorID) {
      // In case of no actorID, the node front had been destroyed.
      // This will occur when the pseudo element was re-generated.
      await this.updateNodeFront(this.props.animation);
    }
  }

  async highlight() {
    await this.ensureNodeFront();

    if (this.state.nodeFront) {
      this.props.dispatch(
        highlightNode(this.state.nodeFront, {
          hideInfoBar: true,
          hideGuides: true,
        })
      );
    }
  }

  async select() {
    await this.ensureNodeFront();

    if (this.state.nodeFront) {
      this.props.setSelectedNode(this.state.nodeFront);
    }
  }

  render() {
    const { dispatch, highlightedNode, setHighlightedNode } = this.props;
    const { nodeFront } = this.state;

    if (!nodeFront) {
      return dom.div({
        className: "animation-target",
      });
    }

    const isHighlighted = nodeFront.actorID === highlightedNode;

    return dom.div(
      {
        className: "animation-target" + (isHighlighted ? " highlighting" : ""),
      },
      Rep({
        defaultRep: ElementNode,
        mode: MODE.TINY,
        inspectIconTitle: getInspectorStr(
          "inspector.nodePreview.highlightNodeLabel"
        ),
        object: translateNodeFrontToGrip(nodeFront),
        onDOMNodeClick: () => this.select(),
        onDOMNodeMouseOut: () => {
          if (!isHighlighted) {
            dispatch(unhighlightNode());
          }
        },
        onDOMNodeMouseOver: () => {
          if (!isHighlighted) {
            this.highlight();
          }
        },
        onInspectIconClick: (_, e) => {
          e.stopPropagation();

          if (!isHighlighted) {
            // At first, hide highlighter which was created by onDOMNodeMouseOver.
            dispatch(unhighlightNode());
          }

          setHighlightedNode(isHighlighted ? null : nodeFront);
        },
      })
    );
  }
}

const mapStateToProps = state => {
  return {
    highlightedNode: state.animations.highlightedNode,
  };
};

module.exports = connect(mapStateToProps)(AnimationTarget);
