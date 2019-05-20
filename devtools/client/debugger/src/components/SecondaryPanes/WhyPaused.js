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
import type { Grip, ExceptionReason } from "../../types";

import "./WhyPaused.css";

type Props = {
  endPanelCollapsed: boolean,
  delay: ?number,
  why: ExceptionReason,
};

type State = {
  hideWhyPaused: string,
};

class WhyPaused extends PureComponent<Props, State> {
  constructor(props) {
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

    const preview = exception.preview;
    if (!preview || !preview.name || !preview.message) {
      return;
    }

    return `${preview.name}: ${preview.message}`;
  }

  renderMessage(why: ExceptionReason) {
    if (why.type == "exception" && why.exception) {
      return (
        <div className={"message warning"}>
          {this.renderExceptionSummary(why.exception)}
        </div>
      );
    }

    if (typeof why.message == "string") {
      return <div className={"message"}>{why.message}</div>;
    }

    return null;
  }

  render() {
    const { endPanelCollapsed, why } = this.props;
    const reason = getPauseReason(why);

    if (reason) {
      if (!endPanelCollapsed) {
        return (
          <div className={"pane why-paused"}>
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
    return <div className={this.state.hideWhyPaused} />;
  }
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  return {
    endPanelCollapsed: getPaneCollapse(state, "end"),
    why: getWhy(state, thread),
  };
};

export default connect(mapStateToProps)(WhyPaused);
