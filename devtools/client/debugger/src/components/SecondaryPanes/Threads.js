/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "../../utils/connect";

import { getAllThreads } from "../../selectors";
import Thread from "./Thread";

import "./Threads.css";

export class Threads extends Component {
  render() {
    const { threads } = this.props;

    return (
      <div className="pane threads-list">
        {threads.map(thread => (
          <Thread thread={thread} key={thread.actor} />
        ))}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  threads: getAllThreads(state),
});

export default connect(mapStateToProps)(Threads);
