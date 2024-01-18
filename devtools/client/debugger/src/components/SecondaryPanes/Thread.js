/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import { div, span } from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";

import actions from "../../actions/index";
import { getCurrentThread, getIsPaused } from "../../selectors/index";
import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("resource://devtools/client/shared/classnames.js");

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
    return div(
      {
        className: classnames("thread", {
          selected: thread.actor == currentThread,
          paused: isPaused,
        }),
        key: thread.actor,
        onClick: this.onSelectThread,
      },
      div(
        {
          className: "icon",
        },
        React.createElement(AccessibleImage, {
          className: isWorker ? "worker" : "window",
        })
      ),
      div(
        {
          className: "label",
        },
        label
      ),
      isPaused
        ? span(
            {
              className: "pause-badge",
              role: "status",
            },
            L10N.getStr("pausedThread")
          )
        : null
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
