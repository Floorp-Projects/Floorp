/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const AnimationDetailContainer = createFactory(require("./AnimationDetailContainer"));
const AnimationListContainer = createFactory(require("./AnimationListContainer"));
const AnimationToolbar = createFactory(require("./AnimationToolbar"));
const NoAnimationPanel = createFactory(require("./NoAnimationPanel"));
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));

class App extends Component {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      detailVisibility: PropTypes.bool.isRequired,
      direction: PropTypes.string.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getAnimationsCurrentTime: PropTypes.func.isRequired,
      getComputedStyle: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      rewindAnimationsCurrentTime: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      setAnimationsCurrentTime: PropTypes.func.isRequired,
      setAnimationsPlaybackRate: PropTypes.func.isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
      setDetailVisibility: PropTypes.func.isRequired,
      setHighlightedNode: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      simulateAnimationForKeyframesProgressBar: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
      toggleElementPicker: PropTypes.func.isRequired,
      toggleLockingHighlight: PropTypes.func.isRequired,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    return this.props.animations.length !== 0 || nextProps.animations.length !== 0;
  }

  render() {
    const {
      addAnimationsCurrentTimeListener,
      animations,
      detailVisibility,
      direction,
      emitEventForTest,
      getAnimatedPropertyMap,
      getAnimationsCurrentTime,
      getComputedStyle,
      getNodeFromActor,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      removeAnimationsCurrentTimeListener,
      rewindAnimationsCurrentTime,
      selectAnimation,
      setAnimationsCurrentTime,
      setAnimationsPlaybackRate,
      setAnimationsPlayState,
      setDetailVisibility,
      setHighlightedNode,
      setSelectedNode,
      simulateAnimation,
      simulateAnimationForKeyframesProgressBar,
      timeScale,
      toggleElementPicker,
    } = this.props;

    return dom.div(
      {
        id: "animation-container",
        className: detailVisibility ? "animation-detail-visible" : "",
        tabIndex: -1,
      },
      animations.length ?
      [
        AnimationToolbar(
          {
            addAnimationsCurrentTimeListener,
            animations,
            removeAnimationsCurrentTimeListener,
            rewindAnimationsCurrentTime,
            setAnimationsPlaybackRate,
            setAnimationsPlayState,
          }
        ),
        SplitBox({
          className: "animation-container-splitter",
          endPanel: AnimationDetailContainer(
            {
              addAnimationsCurrentTimeListener,
              emitEventForTest,
              getAnimatedPropertyMap,
              getAnimationsCurrentTime,
              getComputedStyle,
              removeAnimationsCurrentTimeListener,
              setDetailVisibility,
              simulateAnimation,
              simulateAnimationForKeyframesProgressBar,
              timeScale,
            }
          ),
          endPanelControl: true,
          initialHeight: "50%",
          splitterSize: 1,
          startPanel: AnimationListContainer(
            {
              addAnimationsCurrentTimeListener,
              animations,
              direction,
              emitEventForTest,
              getAnimatedPropertyMap,
              getNodeFromActor,
              onHideBoxModelHighlighter,
              onShowBoxModelHighlighterForNode,
              removeAnimationsCurrentTimeListener,
              selectAnimation,
              setAnimationsCurrentTime,
              setHighlightedNode,
              setSelectedNode,
              simulateAnimation,
              timeScale,
            }
          ),
          vert: false,
        })
      ]
      :
      NoAnimationPanel(
        {
          toggleElementPicker
        }
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    animations: state.animations.animations,
    detailVisibility: state.animations.detailVisibility,
    timeScale: state.animations.timeScale,
  };
};

module.exports = connect(mapStateToProps)(App);
