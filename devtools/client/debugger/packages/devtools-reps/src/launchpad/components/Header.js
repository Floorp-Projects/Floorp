/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
const { Component, createFactory } = React;
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");
const constants = require("../constants");
const QuickLinks = createFactory(require("./QuickLinks"));
require("./Header.css");

class Header extends Component {
  static get propTypes() {
    return {
      addInput: PropTypes.func.isRequired,
      changeCurrentInput: PropTypes.func.isRequired,
      clearResultsList: PropTypes.func.isRequired,
      currentInputValue: PropTypes.string,
      evaluate: PropTypes.func.isRequired,
      navigateInputHistory: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onSubmitForm = this.onSubmitForm.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.onInputKeyDown = this.onInputKeyDown.bind(this);
    this.onClearButtonClick = this.onClearButtonClick.bind(this);
  }

  onSubmitForm(e) {
    e.preventDefault();
    const data = new FormData(e.target);
    const expression = data.get("expression");
    this.props.addInput(expression);
  }

  onInputChange(e) {
    this.props.changeCurrentInput(e.target.value);
  }

  onInputKeyDown(e) {
    if (["ArrowUp", "ArrowDown"].includes(e.key)) {
      this.props.navigateInputHistory(
        e.key === "ArrowUp" ? constants.DIR_BACKWARD : constants.DIR_FORWARD
      );
    }
  }

  onClearButtonClick(e) {
    this.props.clearResultsList();
  }

  render() {
    const { currentInputValue, evaluate } = this.props;

    return dom.header(
      { className: "console-header" },
      dom.form(
        { onSubmit: this.onSubmitForm },
        dom.h1({}, "Reps"),
        dom.input({
          type: "text",
          placeholder: "Enter an expression",
          name: "expression",
          value: currentInputValue || "",
          autoFocus: true,
          onChange: this.onInputChange,
          onKeyDown: this.onInputKeyDown,
        }),
        dom.button(
          {
            className: "clear-button",
            type: "button",
            onClick: this.onClearButtonClick,
          },
          "Clear"
        )
      ),
      QuickLinks({ evaluate })
    );
  }
}

module.exports = Header;
