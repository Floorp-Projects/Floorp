/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "react-redux";
import classnames from "classnames";
import actions from "../../actions";

import { CloseButton } from "../shared/Button";

import "./XHRBreakpoints.css";
import { getXHRBreakpoints, shouldPauseOnAnyXHR } from "../../selectors";
import ExceptionOption from "./Breakpoints/ExceptionOption";

import type { XHRBreakpointsList } from "../../reducers/types";

type Props = {
  xhrBreakpoints: XHRBreakpointsList,
  shouldPauseOnAny: boolean,
  showInput: boolean,
  onXHRAdded: Function,
  setXHRBreakpoint: Function,
  removeXHRBreakpoint: typeof actions.removeXHRBreakpoint,
  enableXHRBreakpoint: typeof actions.enableXHRBreakpoint,
  disableXHRBreakpoint: typeof actions.disableXHRBreakpoint,
  togglePauseOnAny: typeof actions.togglePauseOnAny,
  updateXHRBreakpoint: typeof actions.updateXHRBreakpoint
};

type State = {
  editing: boolean,
  inputValue: string,
  inputMethod: string,
  editIndex: number,
  focused: boolean
};

// At present, the "Pause on any URL" checkbox creates an xhrBreakpoint
// of "ANY" with no path, so we can remove that before creating the list
function getExplicitXHRBreakpoints(xhrBreakpoints) {
  return xhrBreakpoints.filter(bp => bp.path !== "");
}

class XHRBreakpoints extends Component<Props, State> {
  _input: ?HTMLInputElement;

  constructor(props: Props) {
    super(props);

    this.state = {
      editing: false,
      inputValue: "",
      inputMethod: "",
      focused: false,
      editIndex: -1
    };
  }

  componentDidUpdate(prevProps, prevState) {
    const input = this._input;

    if (!input) {
      return;
    }

    if (!prevState.editing && this.state.editing) {
      input.setSelectionRange(0, input.value.length);
      input.focus();
    } else if (this.props.showInput && !this.state.focused) {
      input.focus();
    }
  }

  handleNewSubmit = (e: SyntheticEvent<HTMLFormElement>) => {
    e.preventDefault();
    e.stopPropagation();

    this.props.setXHRBreakpoint(this.state.inputValue, "ANY");

    this.hideInput();
  };

  handleExistingSubmit = (e: SyntheticEvent<HTMLFormElement>) => {
    e.preventDefault();
    e.stopPropagation();

    const { editIndex, inputValue, inputMethod } = this.state;
    const { xhrBreakpoints } = this.props;
    const { path, method } = xhrBreakpoints[editIndex];

    if (path !== inputValue || method != inputMethod) {
      this.props.updateXHRBreakpoint(editIndex, inputValue, inputMethod);
    }

    this.hideInput();
  };

  handleChange = (e: SyntheticInputEvent<HTMLInputElement>) => {
    const target = e.target;
    this.setState({ inputValue: target.value });
  };

  hideInput = () => {
    this.setState({
      focused: false,
      editing: false,
      editIndex: -1,
      inputValue: "",
      inputMethod: ""
    });
    this.props.onXHRAdded();
  };

  onFocus = () => {
    this.setState({ focused: true });
  };

  editExpression = index => {
    const { xhrBreakpoints } = this.props;
    const { path, method } = xhrBreakpoints[index];
    this.setState({
      inputValue: path,
      inputMethod: method,
      editing: true,
      editIndex: index
    });
  };

  renderXHRInput(onSubmit) {
    const { focused, inputValue } = this.state;
    const { showInput } = this.props;
    const placeholder = L10N.getStr("xhrBreakpoints.placeholder");

    return (
      <li
        className={classnames("xhr-input-container", { focused })}
        key="xhr-input"
      >
        <form className="xhr-input-form" onSubmit={onSubmit}>
          <input
            className="xhr-input"
            type="text"
            placeholder={placeholder}
            onChange={this.handleChange}
            onBlur={this.hideInput}
            onFocus={this.onFocus}
            autoFocus={showInput}
            value={inputValue}
            ref={c => (this._input = c)}
          />
          <input type="submit" style={{ display: "none" }} />
        </form>
      </li>
    );
  }
  handleCheckbox = index => {
    const {
      xhrBreakpoints,
      enableXHRBreakpoint,
      disableXHRBreakpoint
    } = this.props;
    const breakpoint = xhrBreakpoints[index];
    if (breakpoint.disabled) {
      enableXHRBreakpoint(index);
    } else {
      disableXHRBreakpoint(index);
    }
  };

  renderBreakpoint = breakpoint => {
    const { path, text, disabled, method } = breakpoint;
    const { editIndex } = this.state;
    const { removeXHRBreakpoint, xhrBreakpoints } = this.props;

    // The "pause on any" checkbox
    if (!path) {
      return;
    }

    // Finds the xhrbreakpoint so as to not make assumptions about position
    const index = xhrBreakpoints.findIndex(
      bp => bp.path === path && bp.method === method
    );

    if (index === editIndex) {
      return this.renderXHRInput(this.handleExistingSubmit);
    }

    return (
      <li
        className="xhr-container"
        key={path}
        title={path}
        onDoubleClick={(items, options) => this.editExpression(index)}
      >
        <input
          type="checkbox"
          className="xhr-checkbox"
          checked={!disabled}
          onChange={() => this.handleCheckbox(index)}
          onClick={ev => ev.stopPropagation()}
        />
        <div className="xhr-label">{text}</div>
        <div className="xhr-container__close-btn">
          <CloseButton handleClick={e => removeXHRBreakpoint(index)} />
        </div>
      </li>
    );
  };

  renderBreakpoints = () => {
    const { showInput, xhrBreakpoints } = this.props;
    const explicitXhrBreakpoints = getExplicitXHRBreakpoints(xhrBreakpoints);

    return (
      <ul className="pane expressions-list">
        {explicitXhrBreakpoints.map(this.renderBreakpoint)}
        {(showInput || explicitXhrBreakpoints.length === 0) &&
          this.renderXHRInput(this.handleNewSubmit)}
      </ul>
    );
  };

  renderCheckbox = () => {
    const { shouldPauseOnAny, togglePauseOnAny, xhrBreakpoints } = this.props;
    const explicitXhrBreakpoints = getExplicitXHRBreakpoints(xhrBreakpoints);

    return (
      <div
        className={classnames("breakpoints-exceptions-options", {
          empty: explicitXhrBreakpoints.length === 0
        })}
      >
        <ExceptionOption
          className="breakpoints-exceptions"
          label={L10N.getStr("pauseOnAnyXHR")}
          isChecked={shouldPauseOnAny}
          onChange={() => togglePauseOnAny()}
        />
      </div>
    );
  };

  render() {
    return (
      <div>
        {this.renderCheckbox()}
        {this.renderBreakpoints()}
      </div>
    );
  }
}

const mapStateToProps = state => {
  return {
    xhrBreakpoints: getXHRBreakpoints(state),
    shouldPauseOnAny: shouldPauseOnAnyXHR(state)
  };
};

export default connect(
  mapStateToProps,
  {
    setXHRBreakpoint: actions.setXHRBreakpoint,
    removeXHRBreakpoint: actions.removeXHRBreakpoint,
    enableXHRBreakpoint: actions.enableXHRBreakpoint,
    disableXHRBreakpoint: actions.disableXHRBreakpoint,
    updateXHRBreakpoint: actions.updateXHRBreakpoint,
    togglePauseOnAny: actions.togglePauseOnAny
  }
)(XHRBreakpoints);
