"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _Button = require("./Button/index");

var _Svg = require("devtools/client/debugger/new/dist/vendors").vendored["Svg"];

var _Svg2 = _interopRequireDefault(_Svg);

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const arrowBtn = (onClick, type, className, tooltip) => {
  const props = {
    className,
    key: type,
    onClick,
    title: tooltip,
    type
  };
  return _react2.default.createElement("button", props, _react2.default.createElement(_Svg2.default, {
    name: type
  }));
};

class SearchInput extends _react.Component {
  constructor(props) {
    super(props);

    this.onFocus = e => {
      const {
        onFocus
      } = this.props;
      this.setState({
        inputFocused: true
      });

      if (onFocus) {
        onFocus(e);
      }
    };

    this.onBlur = e => {
      const {
        onBlur
      } = this.props;
      this.setState({
        inputFocused: false
      });

      if (onBlur) {
        onBlur(e);
      }
    };

    this.onKeyDown = e => {
      const {
        onHistoryScroll,
        onKeyDown
      } = this.props;

      if (!onHistoryScroll) {
        return onKeyDown(e);
      }

      const inputValue = e.target.value;
      const {
        history
      } = this.state;
      const currentHistoryIndex = history.indexOf(inputValue);

      if (e.key === "Enter") {
        this.saveEnteredTerm(inputValue);
        return onKeyDown(e);
      }

      if (e.key === "ArrowUp") {
        const previous = currentHistoryIndex > -1 ? currentHistoryIndex - 1 : history.length - 1;
        const previousInHistory = history[previous];

        if (previousInHistory) {
          e.preventDefault();
          onHistoryScroll(previousInHistory);
        }

        return;
      }

      if (e.key === "ArrowDown") {
        const next = currentHistoryIndex + 1;
        const nextInHistory = history[next];

        if (nextInHistory) {
          onHistoryScroll(nextInHistory);
        }
      }
    };

    this.state = {
      inputFocused: false,
      history: []
    };
  }

  componentDidMount() {
    this.setFocus();
  }

  componentDidUpdate(prevProps) {
    if (this.props.shouldFocus && !prevProps.shouldFocus) {
      this.setFocus();
    }
  }

  setFocus() {
    if (this.$input) {
      const input = this.$input;
      input.focus();

      if (!input.value) {
        return;
      } // omit prefix @:# from being selected


      const selectStartPos = this.props.hasPrefix ? 1 : 0;
      input.setSelectionRange(selectStartPos, input.value.length + 1);
    }
  }

  renderSvg() {
    const svgName = this.props.showErrorEmoji ? "sad-face" : "magnifying-glass";
    return _react2.default.createElement(_Svg2.default, {
      name: svgName
    });
  }

  renderArrowButtons() {
    const {
      handleNext,
      handlePrev
    } = this.props;
    return [arrowBtn(handlePrev, "arrow-up", (0, _classnames2.default)("nav-btn", "prev"), L10N.getFormatStr("editor.searchResults.prevResult")), arrowBtn(handleNext, "arrow-down", (0, _classnames2.default)("nav-btn", "next"), L10N.getFormatStr("editor.searchResults.nextResult"))];
  }

  saveEnteredTerm(query) {
    const {
      history
    } = this.state;
    const previousIndex = history.indexOf(query);

    if (previousIndex !== -1) {
      history.splice(previousIndex, 1);
    }

    history.push(query);
    this.setState({
      history
    });
  }

  renderSummaryMsg() {
    const {
      summaryMsg
    } = this.props;

    if (!summaryMsg) {
      return null;
    }

    return _react2.default.createElement("div", {
      className: "summary"
    }, summaryMsg);
  }

  renderNav() {
    const {
      count,
      handleNext,
      handlePrev
    } = this.props;

    if (!handleNext && !handlePrev || !count || count == 1) {
      return;
    }

    return _react2.default.createElement("div", {
      className: "search-nav-buttons"
    }, this.renderArrowButtons());
  }

  render() {
    const {
      expanded,
      handleClose,
      onChange,
      onKeyUp,
      placeholder,
      query,
      selectedItemId,
      showErrorEmoji,
      size,
      showClose
    } = this.props;
    const inputProps = {
      className: (0, _classnames2.default)({
        empty: showErrorEmoji
      }),
      onChange,
      onKeyDown: e => this.onKeyDown(e),
      onKeyUp,
      onFocus: e => this.onFocus(e),
      onBlur: e => this.onBlur(e),
      "aria-autocomplete": "list",
      "aria-controls": "result-list",
      "aria-activedescendant": expanded && selectedItemId ? `${selectedItemId}-title` : "",
      placeholder,
      value: query,
      spellCheck: false,
      ref: c => this.$input = c
    };
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("search-shadow", {
        focused: this.state.inputFocused
      })
    }, _react2.default.createElement("div", {
      className: (0, _classnames2.default)("search-field", size),
      role: "combobox",
      "aria-haspopup": "listbox",
      "aria-owns": "result-list",
      "aria-expanded": expanded
    }, this.renderSvg(), _react2.default.createElement("input", inputProps), this.renderSummaryMsg(), this.renderNav(), showClose && _react2.default.createElement(_Button.CloseButton, {
      handleClick: handleClose,
      buttonClass: size
    })));
  }

}

SearchInput.defaultProps = {
  expanded: false,
  hasPrefix: false,
  selectedItemId: "",
  size: "",
  showClose: true
};
exports.default = SearchInput;