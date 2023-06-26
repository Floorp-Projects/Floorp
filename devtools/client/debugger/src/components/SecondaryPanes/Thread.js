/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

import actions from "../../actions";
import { getCurrentThread, getIsPaused } from "../../selectors";
import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("devtools/client/shared/classnames.js");

export class Thread extends Component {
  static get propTypes() {
    return {
      currentThread: PropTypes.string.isRequired,
      isPaused: PropTypes.bool.isRequired,
      selectThread: PropTypes.func.isRequired,
      thread: PropTypes.object.isRequired,
    };
  }

  onSelectThread = () => {
    this.props.selectThread(this.props.thread.actor);
  };

  render() {
    const { currentThread, isPaused, thread } = this.props;

    const isWorker = thread.targetType.includes("worker");
    let label = thread.name;
    if (thread.serviceWorkerStatus) {
      label += ` (${thread.serviceWorkerStatus})`;
    }

    return (
      <div
        className={classnames("thread", {
          selected: thread.actor == currentThread,
        })}
        key={thread.actor}
        onClick={this.onSelectThread}
      >
        <div className="icon">
          <AccessibleImage className={isWorker ? "worker" : "window"} />
        </div>
        <div className="label">{label}</div>
        {isPaused ? (
          <div className="pause-badge">
            <AccessibleImage className="pause" />
          </div>
        ) : null}
      </div>
    );
  }
}

const mapStateToProps = (state, props) => ({
  currentThread: getCurrentThread(state),
  isPaused: getIsPaused(state, props.thread.actor),
});

export default connect(mapStateToProps, {
  selectThread: actions.selectThread,
})(Thread);
