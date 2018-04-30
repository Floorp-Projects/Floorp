/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const AnimationTarget = createFactory(require("./AnimationTarget"));
const SummaryGraph = createFactory(require("./graph/SummaryGraph"));

class AnimationItem extends Component {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      selectedAnimation: PropTypes.object.isRequired,
      setHighlightedNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isSelected: false,
    };
  }

  componentWillReceiveProps(nextProps) {
    const { animation } = this.props;

    this.setState({
      isSelected: nextProps.selectedAnimation &&
                  animation.actorID === nextProps.selectedAnimation.actorID
    });
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.state.isSelected !== nextState.isSelected ||
           this.props.animation !== nextProps.animation ||
           this.props.timeScale !== nextProps.timeScale;
  }

  render() {
    const {
      animation,
      emitEventForTest,
      getAnimatedPropertyMap,
      getNodeFromActor,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      selectAnimation,
      setHighlightedNode,
      setSelectedNode,
      simulateAnimation,
      timeScale,
    } = this.props;
    const {
      isSelected,
    } = this.state;

    return dom.li(
      {
        className: `animation-item ${ animation.state.type } ` +
                   (isSelected ? "selected" : ""),
      },
      AnimationTarget(
        {
          animation,
          emitEventForTest,
          getNodeFromActor,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          setHighlightedNode,
          setSelectedNode,
        }
      ),
      SummaryGraph(
        {
          animation,
          emitEventForTest,
          getAnimatedPropertyMap,
          selectAnimation,
          simulateAnimation,
          timeScale,
        }
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedAnimation: state.animations.selectedAnimation,
  };
};

module.exports = connect(mapStateToProps)(AnimationItem);
