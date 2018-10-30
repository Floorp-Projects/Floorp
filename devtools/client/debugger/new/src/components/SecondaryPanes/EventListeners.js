/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import { connect } from "react-redux";
import actions from "../../actions";
import { getEventListeners, getBreakpoint } from "../../selectors";
import { CloseButton } from "../shared/Button";
import "./EventListeners.css";

import type { Breakpoint, Location, SourceId } from "../../types";

type Listener = {
  selector: string,
  type: string,
  sourceId: SourceId,
  line: number,
  breakpoint: ?Breakpoint
};

type Props = {
  listeners: Array<Listener>,
  selectLocation: ({ sourceId: SourceId, line: number }) => void,
  addBreakpoint: ({ sourceId: SourceId, line: number }) => void,
  enableBreakpoint: Location => void,
  disableBreakpoint: Location => void,
  removeBreakpoint: Location => void
};

class EventListeners extends Component<Props> {
  renderListener: Function;

  constructor(...args) {
    super(...args);
  }

  renderListener = ({ type, selector, line, sourceId, breakpoint }) => {
    const checked = breakpoint && !breakpoint.disabled;
    const location = { sourceId, line };

    return (
      <div
        className="listener"
        onClick={() => this.props.selectLocation({ sourceId, line })}
        key={`${type}.${selector}.${sourceId}.${line}`}
      >
        <input
          type="checkbox"
          className="listener-checkbox"
          checked={checked}
          onChange={() => this.handleCheckbox(breakpoint, location)}
        />
        <span className="type">{type}</span>
        <span className="selector">{selector}</span>
        {breakpoint ? (
          <CloseButton
            handleClick={ev => this.removeBreakpoint(ev, breakpoint)}
          />
        ) : (
          ""
        )}
      </div>
    );
  };

  handleCheckbox(breakpoint, location) {
    if (!breakpoint) {
      return this.props.addBreakpoint(location);
    }

    if (breakpoint.loading) {
      return;
    }

    if (breakpoint.disabled) {
      this.props.enableBreakpoint(breakpoint.location);
    } else {
      this.props.disableBreakpoint(breakpoint.location);
    }
  }

  removeBreakpoint(event, breakpoint) {
    event.stopPropagation();
    if (breakpoint) {
      this.props.removeBreakpoint(breakpoint.location);
    }
  }

  render() {
    const { listeners } = this.props;
    return (
      <div className="pane event-listeners">
        {listeners.map(this.renderListener)}
      </div>
    );
  }
}

const mapStateToProps = state => {
  const listeners = getEventListeners(state).map(listener => {
    return {
      ...listener,
      breakpoint: getBreakpoint(state, {
        sourceId: listener.sourceId,
        line: listener.line
      })
    };
  });

  return { listeners };
};

export default connect(
  mapStateToProps,
  {
    selectLocation: actions.selectLocation,
    addBreakpoint: actions.addBreakpoint,
    enableBreakpoint: actions.enableBreakpoint,
    disableBreakpoint: actions.disableBreakpoint,
    removeBreakpoint: actions.removeBreakpoint
  }
)(EventListeners);
