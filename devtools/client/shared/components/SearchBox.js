/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global window */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const AutocompletePopup = createFactory(require("devtools/client/shared/components/AutoCompletePopup"));

class SearchBox extends Component {
  static get propTypes() {
    return {
      delay: PropTypes.number,
      keyShortcut: PropTypes.string,
      onChange: PropTypes.func,
      onFocus: PropTypes.func,
      onBlur: PropTypes.func,
      onKeyDown: PropTypes.func,
      placeholder: PropTypes.string,
      plainStyle: PropTypes.bool,
      type: PropTypes.string,
      autocompleteProvider: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      value: "",
      focused: false,
    };

    this.onChange = this.onChange.bind(this);
    this.onClearButtonClick = this.onClearButtonClick.bind(this);
    this.onFocus = this.onFocus.bind(this);
    this.onBlur = this.onBlur.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  componentDidMount() {
    if (!this.props.keyShortcut) {
      return;
    }

    this.shortcuts = new KeyShortcuts({
      window
    });
    this.shortcuts.on(this.props.keyShortcut, event => {
      event.preventDefault();
      this.refs.input.focus();
    });
  }

  componentWillUnmount() {
    if (this.shortcuts) {
      this.shortcuts.destroy();
    }

    // Clean up an existing timeout.
    if (this.searchTimeout) {
      clearTimeout(this.searchTimeout);
    }
  }

  onChange() {
    if (this.state.value !== this.refs.input.value) {
      this.setState({
        focused: true,
        value: this.refs.input.value,
      });
    }

    if (!this.props.delay) {
      this.props.onChange(this.state.value);
      return;
    }

    // Clean up an existing timeout before creating a new one.
    if (this.searchTimeout) {
      clearTimeout(this.searchTimeout);
    }

    // Execute the search after a timeout. It makes the UX
    // smoother if the user is typing quickly.
    this.searchTimeout = setTimeout(() => {
      this.searchTimeout = null;
      this.props.onChange(this.state.value);
    }, this.props.delay);
  }

  onClearButtonClick() {
    this.refs.input.value = "";
    this.onChange();
  }

  onFocus() {
    if (this.props.onFocus) {
      this.props.onFocus();
    }

    this.setState({ focused: true });
  }

  onBlur() {
    if (this.props.onBlur) {
      this.props.onBlur();
    }

    this.setState({ focused: false });
  }

  onKeyDown(e) {
    if (this.props.onKeyDown) {
      this.props.onKeyDown();
    }

    let { autocomplete } = this.refs;
    if (!autocomplete || autocomplete.state.list.length <= 0) {
      return;
    }

    switch (e.key) {
      case "ArrowDown":
        autocomplete.jumpBy(1);
        break;
      case "ArrowUp":
        autocomplete.jumpBy(-1);
        break;
      case "PageDown":
        autocomplete.jumpBy(5);
        break;
      case "PageUp":
        autocomplete.jumpBy(-5);
        break;
      case "Enter":
      case "Tab":
        e.preventDefault();
        autocomplete.select();
        break;
      case "Escape":
        e.preventDefault();
        this.onBlur();
        break;
      case "Home":
        autocomplete.jumpToTop();
        break;
      case "End":
        autocomplete.jumpToBottom();
        break;
    }
  }

  render() {
    let {
      type = "search",
      placeholder,
      autocompleteProvider,
      plainStyle,
    } = this.props;
    let { value } = this.state;
    let divClassList = ["devtools-searchbox", "has-clear-btn"];
    let inputClassList = [`devtools-${type}input`];
    if (plainStyle) {
      inputClassList.push("devtools-plaininput");
    }
    let showAutocomplete = autocompleteProvider && this.state.focused && value !== "";

    if (value !== "") {
      inputClassList.push("filled");
    }
    return dom.div(
      { className: divClassList.join(" ") },
      dom.input({
        className: inputClassList.join(" "),
        onChange: this.onChange,
        onFocus: this.onFocus,
        onBlur: this.onBlur,
        onKeyDown: this.onKeyDown,
        placeholder,
        ref: "input",
        value,
      }),
      dom.button({
        className: "devtools-searchinput-clear",
        hidden: value == "",
        onClick: this.onClearButtonClick
      }),
      showAutocomplete && AutocompletePopup({
        autocompleteProvider,
        filter: value,
        ref: "autocomplete",
        onItemSelected: (itemValue) => {
          this.setState({ value: itemValue });
          this.onChange();
        }
      })
    );
  }
}

module.exports = SearchBox;
