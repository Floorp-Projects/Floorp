/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { connect } from "../../utils/connect";
import classnames from "classnames";
import actions from "../../actions";

import { CloseButton } from "../shared/Button";

import "./XHRBreakpoints.css";
import { getXHRBreakpoints, shouldPauseOnAnyXHR } from "../../selectors";
import ExceptionOption from "./Breakpoints/ExceptionOption";

// At present, the "Pause on any URL" checkbox creates an xhrBreakpoint
// of "ANY" with no path, so we can remove that before creating the list
function getExplicitXHRBreakpoints(xhrBreakpoints) {
  return xhrBreakpoints.filter(bp => bp.path !== "");
}

const xhrMethods = [
  "ANY",
  "GET",
  "POST",
  "PUT",
  "HEAD",
  "DELETE",
  "PATCH",
  "OPTIONS",
];

class XHRBreakpoints extends Component {
  constructor(props) {
    super(props);

    this.state = {
      editing: false,
      inputValue: "",
      inputMethod: "ANY",
      focused: false,
      editIndex: -1,
      clickedOnFormElement: false,
    };
  }

  componentDidMount() {
    const { showInput } = this.props;

    // Ensures that the input is focused when the "+"
    // is clicked while the panel is collapsed
    if (this._input && showInput) {
      this._input.focus();
    }
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

  handleNewSubmit = e => {
    e.preventDefault();
    e.stopPropagation();

    const setXHRBreakpoint = function() {
      this.props.setXHRBreakpoint(
        this.state.inputValue,
        this.state.inputMethod
      );
      this.hideInput();
    };

    // force update inputMethod in state for mochitest purposes
    // before setting XHR breakpoint
    this.setState(
      { inputMethod: e.target.children[1].value },
      setXHRBreakpoint
    );
  };

  handleExistingSubmit = e => {
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

  handleChange = e => {
    this.setState({ inputValue: e.target.value });
  };

  handleMethodChange = e => {
    this.setState({
      focused: true,
      editing: true,
      inputMethod: e.target.value,
    });
  };

  hideInput = () => {
    if (this.state.clickedOnFormElement) {
      this.setState({
        focused: true,
        clickedOnFormElement: false,
      });
    } else {
      this.setState({
        focused: false,
        editing: false,
        editIndex: -1,
        inputValue: "",
        inputMethod: "ANY",
      });
      this.props.onXHRAdded();
    }
  };

  onFocus = () => {
    this.setState({ focused: true, editing: true });
  };

  onMouseDown = e => {
    this.setState({ editing: false, clickedOnFormElement: true });
  };

  handleTab = e => {
    if (e.key !== "Tab") {
      return;
    }

    if (e.currentTarget.nodeName === "INPUT") {
      this.setState({
        clickedOnFormElement: true,
        editing: false,
      });
    } else if (e.currentTarget.nodeName === "SELECT" && !e.shiftKey) {
      // The user has tabbed off the select and we should
      // cancel the edit
      this.hideInput();
    }
  };

  editExpression = index => {
    const { xhrBreakpoints } = this.props;
    const { path, method } = xhrBreakpoints[index];
    this.setState({
      inputValue: path,
      inputMethod: method,
      editing: true,
      editIndex: index,
    });
  };

  renderXHRInput(onSubmit) {
    const { focused, inputValue } = this.state;
    const placeholder = L10N.getStr("xhrBreakpoints.placeholder");

    return (
      <form
        key="xhr-input-container"
        className={classnames("xhr-input-container xhr-input-form", {
          focused,
        })}
        onSubmit={onSubmit}
      >
        <input
          className="xhr-input-url"
          type="text"
          placeholder={placeholder}
          onChange={this.handleChange}
          onBlur={this.hideInput}
          onFocus={this.onFocus}
          value={inputValue}
          onKeyDown={this.handleTab}
          ref={c => (this._input = c)}
        />
        {this.renderMethodSelectElement()}
        <input type="submit" style={{ display: "none" }} />
      </form>
    );
  }

  handleCheckbox = index => {
    const {
      xhrBreakpoints,
      enableXHRBreakpoint,
      disableXHRBreakpoint,
    } = this.props;
    const breakpoint = xhrBreakpoints[index];
    if (breakpoint.disabled) {
      enableXHRBreakpoint(index);
    } else {
      disableXHRBreakpoint(index);
    }
  };

  renderBreakpoint = breakpoint => {
    const { path, disabled, method } = breakpoint;
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
        key={`${path}-${method}`}
        title={path}
        onDoubleClick={(items, options) => this.editExpression(index)}
      >
        <label>
          <input
            type="checkbox"
            className="xhr-checkbox"
            checked={!disabled}
            onChange={() => this.handleCheckbox(index)}
            onClick={ev => ev.stopPropagation()}
          />
          <div className="xhr-label-method">{method}</div>
          <div className="xhr-label-url">{path}</div>
          <div className="xhr-container__close-btn">
            <CloseButton handleClick={e => removeXHRBreakpoint(index)} />
          </div>
        </label>
      </li>
    );
  };

  renderBreakpoints = explicitXhrBreakpoints => {
    const { showInput } = this.props;

    return (
      <>
        <ul className="pane expressions-list">
          {explicitXhrBreakpoints.map(this.renderBreakpoint)}
        </ul>
        {showInput && this.renderXHRInput(this.handleNewSubmit)}
      </>
    );
  };

  renderCheckbox = explicitXhrBreakpoints => {
    const { shouldPauseOnAny, togglePauseOnAny } = this.props;

    return (
      <div
        className={classnames("breakpoints-exceptions-options", {
          empty: explicitXhrBreakpoints.length === 0,
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

  renderMethodOption = method => {
    return (
      <option
        key={method}
        value={method}
        // e.stopPropagation() required here since otherwise Firefox triggers 2x
        // onMouseDown events on <select> upon clicking on an <option>
        onMouseDown={e => e.stopPropagation()}
      >
        {method}
      </option>
    );
  };

  renderMethodSelectElement = () => {
    return (
      <select
        value={this.state.inputMethod}
        className="xhr-input-method"
        onChange={this.handleMethodChange}
        onMouseDown={this.onMouseDown}
        onKeyDown={this.handleTab}
      >
        {xhrMethods.map(this.renderMethodOption)}
      </select>
    );
  };

  render() {
    const { xhrBreakpoints } = this.props;
    const explicitXhrBreakpoints = getExplicitXHRBreakpoints(xhrBreakpoints);

    return (
      <>
        {this.renderCheckbox(explicitXhrBreakpoints)}
        {explicitXhrBreakpoints.length === 0
          ? this.renderXHRInput(this.handleNewSubmit)
          : this.renderBreakpoints(explicitXhrBreakpoints)}
      </>
    );
  }
}

const mapStateToProps = state => ({
  xhrBreakpoints: getXHRBreakpoints(state),
  shouldPauseOnAny: shouldPauseOnAnyXHR(state),
});

export default connect(mapStateToProps, {
  setXHRBreakpoint: actions.setXHRBreakpoint,
  removeXHRBreakpoint: actions.removeXHRBreakpoint,
  enableXHRBreakpoint: actions.enableXHRBreakpoint,
  disableXHRBreakpoint: actions.disableXHRBreakpoint,
  updateXHRBreakpoint: actions.updateXHRBreakpoint,
  togglePauseOnAny: actions.togglePauseOnAny,
})(XHRBreakpoints);
