/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global window */

"use strict";

const { createFactory, createRef, PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

loader.lazyGetter(this, "AutocompletePopup", function() {
  return createFactory(require("devtools/client/shared/components/AutoCompletePopup"));
});
loader.lazyGetter(this, "MDNLink", function() {
  return createFactory(require("./MdnLink"));
});

loader.lazyRequireGetter(this, "KeyShortcuts", "devtools/client/shared/key-shortcuts");

class SearchBox extends PureComponent {
  static get propTypes() {
    return {
      autocompleteProvider: PropTypes.func,
      delay: PropTypes.number,
      keyShortcut: PropTypes.string,
      learnMoreTitle: PropTypes.string,
      learnMoreUrl: PropTypes.string,
      onBlur: PropTypes.func,
      onChange: PropTypes.func.isRequired,
      onFocus: PropTypes.func,
      onKeyDown: PropTypes.func,
      placeholder: PropTypes.string.isRequired,
      type: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      value: "",
      focused: false,
    };

    this.autocompleteRef = createRef();
    this.inputRef = createRef();

    this.onBlur = this.onBlur.bind(this);
    this.onChange = this.onChange.bind(this);
    this.onClearButtonClick = this.onClearButtonClick.bind(this);
    this.onFocus = this.onFocus.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  componentDidMount() {
    if (!this.props.keyShortcut) {
      return;
    }

    this.shortcuts = new KeyShortcuts({
      window,
    });
    this.shortcuts.on(this.props.keyShortcut, event => {
      event.preventDefault();
      this.inputRef.current.focus();
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
    if (this.state.value !== this.inputRef.current.value) {
      this.setState({
        focused: true,
        value: this.inputRef.current.value,
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
    this.setState({ value: "" });
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

    const autocomplete = this.autocompleteRef.current;
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
    const {
      autocompleteProvider,
      learnMoreTitle,
      learnMoreUrl,
      placeholder,
      type = "search",
    } = this.props;
    const { value } = this.state;
    const showAutocomplete = autocompleteProvider && this.state.focused && value !== "";
    const showLearnMoreLink = learnMoreUrl && value === "";

    const inputClassList = [`devtools-${type}input`];

    return dom.div(
      { className: "devtools-searchbox" },
      dom.input({
        className: inputClassList.join(" "),
        onBlur: this.onBlur,
        onChange: this.onChange,
        onFocus: this.onFocus,
        onKeyDown: this.onKeyDown,
        placeholder,
        ref: this.inputRef,
        value,
      }),
      showLearnMoreLink && MDNLink({
        title: learnMoreTitle,
        url: learnMoreUrl,
      }),
      dom.button({
        className: "devtools-searchinput-clear",
        hidden: value === "",
        onClick: this.onClearButtonClick,
      }),
      showAutocomplete && AutocompletePopup({
        autocompleteProvider,
        filter: value,
        onItemSelected: (itemValue) => {
          this.setState({ value: itemValue });
          this.onChange();
        },
        ref: this.autocompleteRef,
      })
    );
  }
}

module.exports = SearchBox;
