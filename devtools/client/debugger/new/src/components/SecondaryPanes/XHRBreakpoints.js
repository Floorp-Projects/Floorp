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

class XHRBreakpoints extends Component<Props, State> {
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
    const { path, method } = xhrBreakpoints.get(editIndex);

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
    const { path, method } = xhrBreakpoints.get(index);
    this.setState({
      inputValue: path,
      inputMethod: method,
      editing: true,
      editIndex: index
    });
  };

  renderXHRInput(onSubmit) {
    const { focused, inputValue } = this.state;
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
            autoFocus={true}
            value={inputValue}
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
    const breakpoint = xhrBreakpoints.get(index);
    if (breakpoint.disabled) {
      enableXHRBreakpoint(index);
    } else {
      disableXHRBreakpoint(index);
    }
  };

  renderBreakpoint = ({ path, text, disabled, method }, index) => {
    const { editIndex } = this.state;
    const { removeXHRBreakpoint } = this.props;

    if (index === editIndex) {
      return this.renderXHRInput(this.handleExistingSubmit);
    } else if (!path) {
      return;
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
    return (
      <ul className="pane expressions-list">
        {xhrBreakpoints.map(this.renderBreakpoint)}
        {(showInput || !xhrBreakpoints.size) &&
          this.renderXHRInput(this.handleNewSubmit)}
      </ul>
    );
  };

  renderCheckpoint = () => {
    const { shouldPauseOnAny, togglePauseOnAny, xhrBreakpoints } = this.props;
    const isEmpty = xhrBreakpoints.size === 0;
    return (
      <div
        className={classnames("breakpoints-exceptions-options", {
          empty: isEmpty
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
        {this.renderCheckpoint()}
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
