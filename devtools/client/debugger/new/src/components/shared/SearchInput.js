"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _Close = require("./Button/Close");

var _Close2 = _interopRequireDefault(_Close);

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

    this.state = {
      inputFocused: false
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
    return [arrowBtn(handleNext, "arrow-down", (0, _classnames2.default)("nav-btn", "next"), L10N.getFormatStr("editor.searchResults.nextResult")), arrowBtn(handlePrev, "arrow-up", (0, _classnames2.default)("nav-btn", "prev"), L10N.getFormatStr("editor.searchResults.prevResult"))];
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
      onKeyDown,
      onKeyUp,
      placeholder,
      query,
      selectedItemId,
      showErrorEmoji,
      size,
      summaryMsg
    } = this.props;
    const inputProps = {
      className: (0, _classnames2.default)({
        empty: showErrorEmoji
      }),
      onChange,
      onKeyDown,
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
    }, this.renderSvg(), _react2.default.createElement("input", inputProps), summaryMsg && _react2.default.createElement("div", {
      className: "summary"
    }, summaryMsg), this.renderNav(), _react2.default.createElement(_Close2.default, {
      handleClick: handleClose,
      buttonClass: size
    })));
  }

}

SearchInput.defaultProps = {
  expanded: false,
  hasPrefix: false,
  selectedItemId: "",
  size: ""
};
exports.default = SearchInput;