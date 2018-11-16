/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { connect } from "react-redux";
import type { List } from "immutable";

import "./Workers.css";

import actions from "../../actions";
import { getWorkers } from "../../selectors";
import { basename } from "../../utils/path";
import type { Worker } from "../../types";

export class Workers extends PureComponent {
  props: {
    workers: List<Worker>,
    openWorkerToolbox: object => void
  };

  renderWorkers(workers) {
    const { openWorkerToolbox } = this.props;
    return workers.map(worker => (
      <div
        className="worker"
        key={worker.actor}
        onClick={() => openWorkerToolbox(worker)}
      >
        <img className="domain" />
        {basename(worker.url)}
      </div>
    ));
  }

  renderNoWorkersPlaceholder() {
    return <div className="pane-info">{L10N.getStr("noWorkersText")}</div>;
  }

  render() {
    const { workers } = this.props;
    return (
      <div className="pane workers-list">
        {workers && workers.size > 0
          ? this.renderWorkers(workers)
          : this.renderNoWorkersPlaceholder()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  workers: getWorkers(state)
});

export default connect(
  mapStateToProps,
  actions
)(Workers);
