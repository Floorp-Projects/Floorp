/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "../../utils/connect";
import classnames from "classnames";

import actions from "../../actions";
import { getCurrentThread, getIsPaused, getContext } from "../../selectors";
import { getDisplayName, isWorker } from "../../utils/threads";
import AccessibleImage from "../shared/AccessibleImage";

import type { Context, Thread } from "../../types";

type Props = {
  cx: Context,
  selectThread: typeof actions.selectThread,
  isPaused: boolean,
  thread: Thread,
  currentThread: string,
};

export class Worker extends Component<Props> {
  onSelectThread = () => {
    const { thread } = this.props;
    this.props.selectThread(this.props.cx, thread.actor);
  };

  render() {
    const { currentThread, isPaused, thread } = this.props;

    const worker = isWorker(thread);
    const label = worker ? getDisplayName(thread) : L10N.getStr("mainThread");

    return (
      <div
        className={classnames("worker", {
          selected: thread.actor == currentThread,
        })}
        key={thread.actor}
        onClick={this.onSelectThread}
      >
        <div className="icon">
          <AccessibleImage className={worker ? "worker" : "window"} />
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

const mapStateToProps = (state, props: Props) => ({
  cx: getContext(state),
  currentThread: getCurrentThread(state),
  isPaused: getIsPaused(state, props.thread.actor),
});

export default connect(
  mapStateToProps,
  {
    selectThread: actions.selectThread,
  }
)(Worker);
