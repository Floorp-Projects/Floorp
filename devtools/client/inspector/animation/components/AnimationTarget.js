/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

class AnimationTarget extends PureComponent {
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
    return this.state.nodeFront !== nextState.nodeFront;
  }

  /**
   * While waiting for a reps fix in https://github.com/devtools-html/reps/issues/92,
   * translate nodeFront to a grip-like object that can be used with an ElementNode rep.
   *
   * @params  {NodeFront} nodeFront
   *          The NodeFront for which we want to create a grip-like object.
   * @returns {Object} a grip-like object that can be used with Reps.
   */
  translateNodeFrontToGrip(nodeFront) {
    let { attributes } = nodeFront;

    // The main difference between NodeFront and grips is that attributes are treated as
    // a map in grips and as an array in NodeFronts.
    let attributesMap = {};
    for (let {name, value} of attributes) {
      attributesMap[name] = value;
    }

    return {
      actor: nodeFront.actorID,
      preview: {
        attributes: attributesMap,
        attributesLength: attributes.length,
        isConnected: true,
        nodeName: nodeFront.nodeName.toLowerCase(),
        nodeType: nodeFront.nodeType,
      }
    };
  }

  async updateNodeFront(animation) {
    const { emitEventForTest, getNodeFromActor } = this.props;

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
        return;
      }
    }

    this.setState({ nodeFront });
    emitEventForTest("animation-target-rendered");
  }

  render() {
    const {
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      setSelectedNode,
    } = this.props;

    const { nodeFront } = this.state;

    if (!nodeFront) {
      return dom.div(
        {
          className: "animation-target"
        }
      );
    }

    return dom.div(
      {
        className: "animation-target"
      },
      Rep(
        {
          defaultRep: ElementNode,
          mode: MODE.TINY,
          object: this.translateNodeFrontToGrip(nodeFront),
          onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
          onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(nodeFront),
          onInspectIconClick: () => setSelectedNode(nodeFront, "animation-panel"),
        }
      )
    );
  }
}

module.exports = AnimationTarget;
