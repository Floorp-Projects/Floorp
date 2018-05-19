"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _devtoolsReps = require("devtools/client/shared/components/reps/reps.js");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _expressions = require("../../utils/expressions");

var _firefox = require("../../client/firefox");

var _Button = require("../shared/Button/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Expressions extends _react.Component {
  constructor(props) {
    super(props);

    this.clear = () => {
      this.setState(() => {
        this.props.clearExpressionError();
        return {
          editing: false,
          editIndex: -1,
          inputValue: ""
        };
      });
    };

    this.handleChange = e => {
      this.setState({
        inputValue: e.target.value
      });
    };

    this.handleKeyDown = e => {
      if (e.key === "Escape") {
        this.clear();
      }
    };

    this.hideInput = () => {
      this.setState({
        focused: false
      });
      this.props.onExpressionAdded();
    };

    this.onFocus = () => {
      this.setState({
        focused: true
      });
    };

    this.handleExistingSubmit = async (e, expression) => {
      e.preventDefault();
      e.stopPropagation();
      this.props.updateExpression(this.state.inputValue, expression);
      this.hideInput();
    };

    this.handleNewSubmit = async e => {
      const {
        inputValue
      } = this.state;
      e.preventDefault();
      e.stopPropagation();
      this.props.clearExpressionError();
      await this.props.addExpression(this.state.inputValue);
      this.setState({
        editing: false,
        editIndex: -1,
        inputValue: this.props.expressionError ? inputValue : ""
      });

      if (!this.props.expressionError) {
        this.hideInput();
      }
    };

    this.renderExpression = (expression, index) => {
      const {
        expressionError,
        openLink
      } = this.props;
      const {
        editing,
        editIndex
      } = this.state;
      const {
        input,
        updating
      } = expression;
      const isEditingExpr = editing && editIndex === index;

      if (isEditingExpr || isEditingExpr && expressionError) {
        return this.renderExpressionEditInput(expression);
      }

      if (updating) {
        return;
      }

      const {
        value
      } = (0, _expressions.getValue)(expression);
      const root = {
        name: expression.input,
        path: input,
        contents: {
          value
        }
      };
      return _react2.default.createElement("li", {
        className: "expression-container",
        key: input,
        title: expression.input,
        onDoubleClick: (items, options) => this.editExpression(expression, index)
      }, _react2.default.createElement("div", {
        className: "expression-content"
      }, _react2.default.createElement(_devtoolsReps.ObjectInspector, {
        roots: [root],
        autoExpandDepth: 0,
        disableWrap: true,
        focusable: false,
        openLink: openLink,
        createObjectClient: grip => (0, _firefox.createObjectClient)(grip)
      }), _react2.default.createElement("div", {
        className: "expression-container__close-btn"
      }, _react2.default.createElement(_Button.CloseButton, {
        handleClick: e => this.deleteExpression(e, expression)
      }))));
    };

    this.state = {
      editing: false,
      editIndex: -1,
      inputValue: "",
      focused: false
    };
  }

  componentDidMount() {
    const {
      expressions,
      evaluateExpressions
    } = this.props;

    if (expressions.size > 0) {
      evaluateExpressions();
    }
  }

  componentWillReceiveProps(nextProps) {
    if (this.state.editing && !nextProps.expressionError) {
      this.clear();
    }
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      editing,
      inputValue,
      focused
    } = this.state;
    const {
      expressions,
      expressionError,
      showInput
    } = this.props;
    return expressions !== nextProps.expressions || expressionError !== nextProps.expressionError || editing !== nextState.editing || inputValue !== nextState.inputValue || nextProps.showInput !== showInput || focused !== nextState.focused;
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

  editExpression(expression, index) {
    this.setState({
      inputValue: expression.input,
      editing: true,
      editIndex: index
    });
  }

  deleteExpression(e, expression) {
    e.stopPropagation();
    const {
      deleteExpression
    } = this.props;
    deleteExpression(expression);
  }

  onBlur() {
    this.clear();
    this.hideInput();
  }

  renderNewExpressionInput() {
    const {
      expressionError,
      showInput
    } = this.props;
    const {
      editing,
      inputValue,
      focused
    } = this.state;
    const error = editing === false && expressionError === true;
    const placeholder = error ? L10N.getStr("expressions.errorMsg") : L10N.getStr("expressions.placeholder");
    return _react2.default.createElement("li", {
      className: (0, _classnames2.default)("expression-input-container", {
        focused,
        error
      })
    }, _react2.default.createElement("form", {
      className: "expression-input-form",
      onSubmit: this.handleNewSubmit
    }, _react2.default.createElement("input", {
      className: "input-expression",
      type: "text",
      placeholder: placeholder,
      onChange: this.handleChange,
      onBlur: this.hideInput,
      onKeyDown: this.handleKeyDown,
      onFocus: this.onFocus,
      autoFocus: showInput,
      value: !editing ? inputValue : "",
      ref: c => this._input = c
    }), _react2.default.createElement("input", {
      type: "submit",
      style: {
        display: "none"
      }
    })));
  }

  renderExpressionEditInput(expression) {
    const {
      expressionError
    } = this.props;
    const {
      inputValue,
      editing,
      focused
    } = this.state;
    const error = editing === true && expressionError === true;
    return _react2.default.createElement("span", {
      className: (0, _classnames2.default)("expression-input-container", {
        focused,
        error
      }),
      key: expression.input
    }, _react2.default.createElement("form", {
      className: "expression-input-form",
      onSubmit: e => this.handleExistingSubmit(e, expression)
    }, _react2.default.createElement("input", {
      className: (0, _classnames2.default)("input-expression", {
        error
      }),
      type: "text",
      onChange: this.handleChange,
      onBlur: this.clear,
      onKeyDown: this.handleKeyDown,
      onFocus: this.onFocus,
      value: editing ? inputValue : expression.input,
      ref: c => this._input = c
    }), _react2.default.createElement("input", {
      type: "submit",
      style: {
        display: "none"
      }
    })));
  }

  render() {
    const {
      expressions,
      showInput
    } = this.props;
    return _react2.default.createElement("ul", {
      className: "pane expressions-list"
    }, expressions.map(this.renderExpression), (showInput || !expressions.size) && this.renderNewExpressionInput());
  }

}

const mapStateToProps = state => ({
  expressions: (0, _selectors.getExpressions)(state),
  expressionError: (0, _selectors.getExpressionError)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Expressions);