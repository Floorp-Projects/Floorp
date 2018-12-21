/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "react-redux";
import type { List } from "immutable";

import "./Workers.css";

import actions from "../../actions";
import {
  getMainThread,
  getCurrentThread,
  threadIsPaused,
  getWorkers
} from "../../selectors";
import { basename } from "../../utils/path";
import { features } from "../../utils/prefs";
import type { Worker } from "../../types";
import AccessibleImage from "../shared/AccessibleImage";
import classnames from "classnames";

type Props = {
  selectThread: string => void
};

export class Workers extends Component<Props> {
  props: {
    workers: List<Worker>,
    openWorkerToolbox: object => void,
    mainThread: string,
    currentThread: string
  };

  renderWorkers(workers, mainThread, currentThread) {
    if (features.windowlessWorkers) {
      return [{ actor: mainThread }, ...workers].map(worker => (
        <div
          className={classnames(
            "worker",
            worker.actor == currentThread && "selected"
          )}
          key={worker.actor}
          onClick={() => this.props.selectThread(worker.actor)}
        >
          <img className="domain" />
          {(worker.url ? basename(worker.url) : "Main Thread") +
            (this.props.threadIsPaused(worker.actor) ? " PAUSED" : "")}
        </div>
      ));
    }
    const { openWorkerToolbox } = this.props;
    return workers.map(worker => (
      <div
        className="worker"
        key={worker.actor}
        onClick={() => openWorkerToolbox(worker)}
      >
        <AccessibleImage className="domain" />
        {basename(worker.url)}
      </div>
    ));
  }

  renderNoWorkersPlaceholder() {
    return <div className="pane-info">{L10N.getStr("noWorkersText")}</div>;
  }

  render() {
    const { workers, mainThread, currentThread } = this.props;
    return (
      <div className="pane workers-list">
        {workers && workers.size > 0
          ? this.renderWorkers(workers, mainThread, currentThread)
          : this.renderNoWorkersPlaceholder()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  workers: getWorkers(state),
  mainThread: getMainThread(state),
  currentThread: getCurrentThread(state),
  threadIsPaused: thread => threadIsPaused(state, thread)
});

export default connect(
  mapStateToProps,
  {
    openWorkerToolbox: actions.openWorkerToolbox,
    selectThread: actions.selectThread
  }
)(Workers);
