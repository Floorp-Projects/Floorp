/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const AnimationDetailContainer = createFactory(require("./AnimationDetailContainer"));
const AnimationListContainer = createFactory(require("./AnimationListContainer"));
const AnimationToolbar = createFactory(require("./AnimationToolbar"));
const NoAnimationPanel = createFactory(require("./NoAnimationPanel"));
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));

class App extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      detailVisibility: PropTypes.bool.isRequired,
      emitEventForTest: PropTypes.func.isRequired,
      getAnimatedPropertyMap: PropTypes.func.isRequired,
      getComputedStyle: PropTypes.func.isRequired,
      getNodeFromActor: PropTypes.func.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      rewindAnimationsCurrentTime: PropTypes.func.isRequired,
      selectAnimation: PropTypes.func.isRequired,
      setAnimationsPlaybackRate: PropTypes.func.isRequired,
      setAnimationsPlayState: PropTypes.func.isRequired,
      setDetailVisibility: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
      toggleElementPicker: PropTypes.func.isRequired,
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
      emitEventForTest,
      getAnimatedPropertyMap,
      getComputedStyle,
      getNodeFromActor,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
      removeAnimationsCurrentTimeListener,
      rewindAnimationsCurrentTime,
      selectAnimation,
      setAnimationsPlaybackRate,
      setAnimationsPlayState,
      setDetailVisibility,
      setSelectedNode,
      simulateAnimation,
      timeScale,
      toggleElementPicker,
    } = this.props;

    return dom.div(
      {
        id: "animation-container",
        className: detailVisibility ? "animation-detail-visible" : "",
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
              emitEventForTest,
              getAnimatedPropertyMap,
              getComputedStyle,
              setDetailVisibility,
              simulateAnimation,
            }
          ),
          endPanelControl: true,
          initialHeight: "50%",
          splitterSize: 1,
          startPanel: AnimationListContainer(
            {
              animations,
              emitEventForTest,
              getAnimatedPropertyMap,
              getNodeFromActor,
              onHideBoxModelHighlighter,
              onShowBoxModelHighlighterForNode,
              selectAnimation,
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
