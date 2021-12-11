/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class CurrentTimeScrubber extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      direction: PropTypes.string.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      setAnimationsCurrentTime: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this._ref = createRef();

    const { addAnimationsCurrentTimeListener } = props;
    this.onCurrentTimeUpdated = this.onCurrentTimeUpdated.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);

    addAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  componentDidMount() {
    const current = this._ref.current;
    current.addEventListener("mousedown", this.onMouseDown);
    this._scrubber = current.querySelector(".current-time-scrubber");
  }

  componentWillUnmount() {
    const { removeAnimationsCurrentTimeListener } = this.props;
    removeAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  onCurrentTimeUpdated(currentTime) {
    const { timeScale } = this.props;

    const position = currentTime / timeScale.getDuration();
    // onCurrentTimeUpdated is bound to requestAnimationFrame.
    // As to update the component too frequently has performance issue if React controlled,
    // update raw component directly. See Bug 1699039.
    this._scrubber.style.marginInlineStart = `${position * 100}%`;
  }

  onMouseDown(event) {
    event.stopPropagation();
    const current = this._ref.current;
    this.controllerArea = current.getBoundingClientRect();
    this.listenerTarget = DevToolsUtils.getTopWindow(current.ownerGlobal);
    this.listenerTarget.addEventListener("mousemove", this.onMouseMove);
    this.listenerTarget.addEventListener("mouseup", this.onMouseUp);
    this.decorationTarget = current.closest(".animation-list-container");
    this.decorationTarget.classList.add("active-scrubber");

    this.updateAnimationsCurrentTime(event.pageX, true);
  }

  onMouseMove(event) {
    event.stopPropagation();
    this.isMouseMoved = true;
    this.updateAnimationsCurrentTime(event.pageX);
  }

  onMouseUp(event) {
    event.stopPropagation();

    if (this.isMouseMoved) {
      this.updateAnimationsCurrentTime(event.pageX, true);
      this.isMouseMoved = null;
    }

    this.uninstallListeners();
  }

  uninstallListeners() {
    this.listenerTarget.removeEventListener("mousemove", this.onMouseMove);
    this.listenerTarget.removeEventListener("mouseup", this.onMouseUp);
    this.listenerTarget = null;
    this.decorationTarget.classList.remove("active-scrubber");
    this.decorationTarget = null;
    this.controllerArea = null;
  }

  updateAnimationsCurrentTime(pageX, needRefresh) {
    const { direction, setAnimationsCurrentTime, timeScale } = this.props;

    let progressRate =
      (pageX - this.controllerArea.x) / this.controllerArea.width;

    if (progressRate < 0.0) {
      progressRate = 0.0;
    } else if (progressRate > 1.0) {
      progressRate = 1.0;
    }

    const time =
      direction === "ltr"
        ? progressRate * timeScale.getDuration()
        : (1 - progressRate) * timeScale.getDuration();

    setAnimationsCurrentTime(time, needRefresh);
  }

  render() {
    return dom.div(
      {
        className: "current-time-scrubber-area",
        ref: this._ref,
      },
      dom.div({ className: "indication-bar current-time-scrubber" })
    );
  }
}

module.exports = CurrentTimeScrubber;
