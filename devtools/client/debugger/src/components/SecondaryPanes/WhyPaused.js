/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "../../utils/connect";
import AccessibleImage from "../shared/AccessibleImage";

import { getPauseReason } from "../../utils/pause";
import {
  getCurrentThread,
  getPaneCollapse,
  getPauseReason as getWhy,
} from "../../selectors";
import type { Grip, Why } from "../../types";

import "./WhyPaused.css";

type OwnProps = {|
  +delay?: number,
|};
type Props = {
  endPanelCollapsed: boolean,
  +delay: ?number,
  why: ?Why,
};

type State = {
  hideWhyPaused: string,
};

class WhyPaused extends PureComponent<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = { hideWhyPaused: "" };
  }

  componentDidUpdate() {
    const { delay } = this.props;

    if (delay) {
      setTimeout(() => {
        this.setState({ hideWhyPaused: "" });
      }, delay);
    } else {
      this.setState({ hideWhyPaused: "pane why-paused" });
    }
  }

  renderExceptionSummary(exception: string | Grip) {
    if (typeof exception === "string") {
      return exception;
    }

    const { preview } = exception;
    if (!preview || !preview.name || !preview.message) {
      return;
    }

    return `${preview.name}: ${preview.message}`;
  }

  renderMessage(why: Why) {
    if (why.type == "exception" && why.exception) {
      // Our types for 'Why' are too general because 'type' can be 'string'.
      // $FlowFixMe - We should have a proper discriminating union of reasons.
      const summary = this.renderExceptionSummary(why.exception);
      return <div className="message warning">{summary}</div>;
    }

    if (typeof why.message == "string") {
      return <div className="message">{why.message}</div>;
    }

    return null;
  }

  render() {
    const { endPanelCollapsed, why } = this.props;
    const reason = getPauseReason(why);

    if (!why || !reason || endPanelCollapsed) {
      return <div className={this.state.hideWhyPaused} />;
    }

    return (
      <div className="pane why-paused">
        <div>
          <div className="pause reason">
            {L10N.getStr(reason)}
            {this.renderMessage(why)}
          </div>
          <div className="info icon">
            <AccessibleImage className="info" />
          </div>
        </div>
      </div>
    );
  }
}

const mapStateToProps = state => ({
  endPanelCollapsed: getPaneCollapse(state, "end"),
  why: getWhy(state, getCurrentThread(state)),
});

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps)(WhyPaused);
