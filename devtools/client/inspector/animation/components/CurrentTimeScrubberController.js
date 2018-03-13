/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const CurrentTimeScrubber = createFactory(require("./CurrentTimeScrubber"));

class CurrentTimeScrubberController extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      setAnimationsCurrentTime: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    const { addAnimationsCurrentTimeListener } = props;
    this.onCurrentTimeUpdated = this.onCurrentTimeUpdated.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseOut = this.onMouseOut.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);

    this.state = {
      // offset of the position for the scrubber
      offset: 0,
    };

    addAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  componentDidMount() {
    const parentEl = ReactDOM.findDOMNode(this).parentElement;
    parentEl.addEventListener("mousedown", this.onMouseDown);
  }

  componentWillUnmount() {
    const { removeAnimationsCurrentTimeListener } = this.props;
    removeAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  onCurrentTimeUpdated(currentTime) {
    const { timeScale } = this.props;

    const thisEl = ReactDOM.findDOMNode(this);
    const offset =
      thisEl ? currentTime / timeScale.getDuration() * thisEl.clientWidth : 0;
    this.setState({ offset });
  }

  onMouseDown(e) {
    const thisEl = ReactDOM.findDOMNode(this);
    this.controllerArea = thisEl.getBoundingClientRect();
    this.listenerTarget = thisEl.closest(".animation-list-container");
    this.listenerTarget.addEventListener("mousemove", this.onMouseMove);
    this.listenerTarget.addEventListener("mouseout", this.onMouseOut);
    this.listenerTarget.addEventListener("mouseup", this.onMouseUp);
    this.listenerTarget.classList.add("active-scrubber");

    this.updateAnimationsCurrentTime(e.pageX, true);
  }

  onMouseMove(e) {
    this.isMouseMoved = true;
    this.updateAnimationsCurrentTime(e.pageX);
  }

  onMouseOut(e) {
    if (!this.listenerTarget.contains(e.relatedTarget)) {
      const endX = this.controllerArea.x + this.controllerArea.width;
      const pageX = endX < e.pageX ? endX : e.pageX;
      this.updateAnimationsCurrentTime(pageX, true);
      this.uninstallListeners();
    }
  }

  onMouseUp(e) {
    if (this.isMouseMoved) {
      this.updateAnimationsCurrentTime(e.pageX, true);
      this.isMouseMoved = null;
    }

    this.uninstallListeners();
  }

  uninstallListeners() {
    this.listenerTarget.removeEventListener("mousemove", this.onMouseMove);
    this.listenerTarget.removeEventListener("mouseout", this.onMouseOut);
    this.listenerTarget.removeEventListener("mouseup", this.onMouseUp);
    this.listenerTarget.classList.remove("active-scrubber");
    this.listenerTarget = null;
    this.controllerArea = null;
  }

  updateAnimationsCurrentTime(pageX, needRefresh) {
    const {
      setAnimationsCurrentTime,
      timeScale,
    } = this.props;

    const time = pageX - this.controllerArea.x < 0 ?
                   0 :
                   (pageX - this.controllerArea.x) /
                     this.controllerArea.width * timeScale.getDuration();

    setAnimationsCurrentTime(time, needRefresh);
  }

  render() {
    const { offset } = this.state;

    return dom.div(
      {
        className: "current-time-scrubber-controller devtools-toolbar",
      },
      CurrentTimeScrubber(
        {
          offset,
        }
      )
    );
  }
}

module.exports = CurrentTimeScrubberController;
