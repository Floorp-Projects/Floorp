/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const IndicationBar = createFactory(require("./IndicationBar"));

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

    const { addAnimationsCurrentTimeListener } = props;
    this.onCurrentTimeUpdated = this.onCurrentTimeUpdated.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);

    this.state = {
      // position for the scrubber
      position: 0,
    };

    addAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  componentDidMount() {
    const el = ReactDOM.findDOMNode(this);
    el.addEventListener("mousedown", this.onMouseDown);
  }

  componentWillUnmount() {
    const { removeAnimationsCurrentTimeListener } = this.props;
    removeAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  onCurrentTimeUpdated(currentTime) {
    const { timeScale } = this.props;

    const position = currentTime / timeScale.getDuration();
    this.setState({ position });
  }

  onMouseDown(event) {
    event.stopPropagation();
    const thisEl = ReactDOM.findDOMNode(this);
    this.controllerArea = thisEl.getBoundingClientRect();
    this.listenerTarget = DevToolsUtils.getTopWindow(thisEl.ownerGlobal);
    this.listenerTarget.addEventListener("mousemove", this.onMouseMove);
    this.listenerTarget.addEventListener("mouseup", this.onMouseUp);
    this.decorationTarget = thisEl.closest(".animation-list-container");
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
    const {
      direction,
      setAnimationsCurrentTime,
      timeScale,
    } = this.props;

    let progressRate = (pageX - this.controllerArea.x) / this.controllerArea.width;

    if (progressRate < 0.0) {
      progressRate = 0.0;
    } else if (progressRate > 1.0) {
      progressRate = 1.0;
    }

    const time = direction === "ltr"
                  ? progressRate * timeScale.getDuration()
                  : (1 - progressRate) * timeScale.getDuration();

    setAnimationsCurrentTime(time, needRefresh);
  }

  render() {
    const { position } = this.state;

    return dom.div(
      {
        className: "current-time-scrubber-area",
      },
      IndicationBar(
        {
          className: "current-time-scrubber",
          position,
        }
      )
    );
  }
}

module.exports = CurrentTimeScrubber;
