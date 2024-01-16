/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import {
  div,
  input,
  li,
  ul,
  form,
  datalist,
  option,
  span,
} from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";
import { features } from "../../utils/prefs";
import AccessibleImage from "../shared/AccessibleImage";

import { objectInspector } from "devtools/client/shared/components/reps/index";

import actions from "../../actions";
import {
  getExpressions,
  getExpressionError,
  getAutocompleteMatchset,
  getSelectedSource,
  isMapScopesEnabled,
  getIsCurrentThreadPaused,
  getSelectedFrame,
  getOriginalFrameScope,
  getCurrentThread,
} from "../../selectors";
import { getExpressionResultGripAndFront } from "../../utils/expressions";

import { CloseButton } from "../shared/Button";

import "./Expressions.css";

const { debounce } = require("devtools/shared/debounce");
const classnames = require("devtools/client/shared/classnames.js");

const { ObjectInspector } = objectInspector;

class Expressions extends Component {
  constructor(props) {
    super(props);

    this.state = {
      editing: false,
      editIndex: -1,
      inputValue: "",
      focused: false,
    };
  }

  static get propTypes() {
    return {
      addExpression: PropTypes.func.isRequired,
      autocomplete: PropTypes.func.isRequired,
      autocompleteMatches: PropTypes.array,
      clearAutocomplete: PropTypes.func.isRequired,
      clearExpressionError: PropTypes.func.isRequired,
      deleteExpression: PropTypes.func.isRequired,
      expressionError: PropTypes.bool.isRequired,
      expressions: PropTypes.array.isRequired,
      highlightDomElement: PropTypes.func.isRequired,
      onExpressionAdded: PropTypes.func.isRequired,
      openElementInInspector: PropTypes.func.isRequired,
      openLink: PropTypes.any.isRequired,
      showInput: PropTypes.bool.isRequired,
      unHighlightDomElement: PropTypes.func.isRequired,
      updateExpression: PropTypes.func.isRequired,
      isOriginalVariableMappingDisabled: PropTypes.bool,
      isLoadingOriginalVariables: PropTypes.bool,
    };
  }

  componentDidMount() {
    const { showInput } = this.props;

    // Ensures that the input is focused when the "+"
    // is clicked while the panel is collapsed
    if (showInput && this._input) {
      this._input.focus();
    }
  }

  clear = () => {
    this.setState(() => {
      this.props.clearExpressionError();
      return { editing: false, editIndex: -1, inputValue: "", focused: false };
    });
  };

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    if (this.state.editing && !nextProps.expressionError) {
      this.clear();
    }

    // Ensures that the add watch expression input
    // is no longer visible when the new watch expression is rendered
    if (this.props.expressions.length < nextProps.expressions.length) {
      this.hideInput();
    }
  }

  shouldComponentUpdate(nextProps, nextState) {
    const { editing, inputValue, focused } = this.state;
    const {
      expressions,
      expressionError,
      showInput,
      autocompleteMatches,
      isLoadingOriginalVariables,
      isOriginalVariableMappingDisabled,
    } = this.props;

    return (
      autocompleteMatches !== nextProps.autocompleteMatches ||
      expressions !== nextProps.expressions ||
      expressionError !== nextProps.expressionError ||
      isLoadingOriginalVariables !== nextProps.isLoadingOriginalVariables ||
      isOriginalVariableMappingDisabled !==
        nextProps.isOriginalVariableMappingDisabled ||
      editing !== nextState.editing ||
      inputValue !== nextState.inputValue ||
      nextProps.showInput !== showInput ||
      focused !== nextState.focused
    );
  }

  componentDidUpdate(prevProps, prevState) {
    const _input = this._input;

    if (!_input) {
      return;
    }

    if (!prevState.editing && this.state.editing) {
      _input.setSelectionRange(0, _input.value.length);
      _input.focus();
    } else if (this.props.showInput && !this.state.focused) {
      _input.focus();
    }
  }

  editExpression(expression, index) {
    this.setState({
      inputValue: expression.input,
      editing: true,
      editIndex: index,
    });
  }

  deleteExpression(e, expression) {
    e.stopPropagation();
    const { deleteExpression } = this.props;
    deleteExpression(expression);
  }

  handleChange = e => {
    const { target } = e;
    if (features.autocompleteExpression) {
      this.findAutocompleteMatches(target.value, target.selectionStart);
    }
    this.setState({ inputValue: target.value });
  };

  findAutocompleteMatches = debounce((value, selectionStart) => {
    const { autocomplete } = this.props;
    autocomplete(value, selectionStart);
  }, 250);

  handleKeyDown = e => {
    if (e.key === "Escape") {
      this.clear();
    }
  };

  hideInput = () => {
    this.setState({ focused: false });
    this.props.onExpressionAdded();
    this.props.clearExpressionError();
  };

  createElement = element => {
    return document.createElement(element);
  };

  onFocus = () => {
    this.setState({ focused: true });
  };

  onBlur() {
    this.clear();
    this.hideInput();
  }

  handleExistingSubmit = async (e, expression) => {
    e.preventDefault();
    e.stopPropagation();

    this.props.updateExpression(this.state.inputValue, expression);
  };

  handleNewSubmit = async e => {
    const { inputValue } = this.state;
    e.preventDefault();
    e.stopPropagation();

    this.props.clearExpressionError();
    await this.props.addExpression(this.state.inputValue);
    this.setState({
      editing: false,
      editIndex: -1,
      inputValue: this.props.expressionError ? inputValue : "",
    });

    this.props.clearAutocomplete();
  };

  renderExpressionsNotification() {
    const { isOriginalVariableMappingDisabled, isLoadingOriginalVariables } =
      this.props;

    if (isOriginalVariableMappingDisabled) {
      return div(
        {
          className: "pane-info no-original-scopes-info",
          "aria-role": "status",
        },
        span(
          { className: "info icon" },
          React.createElement(AccessibleImage, { className: "sourcemap" })
        ),
        span(
          { className: "message" },
          L10N.getStr("expressions.noOriginalScopes")
        )
      );
    }

    if (isLoadingOriginalVariables) {
      return div(
        { className: "pane-info" },
        span(
          { className: "info icon" },
          React.createElement(AccessibleImage, { className: "loader" })
        ),
        span(
          { className: "message" },
          L10N.getStr("scopes.loadingOriginalScopes")
        )
      );
    }
    return null;
  }

  renderExpression = (expression, index) => {
    const {
      expressionError,
      openLink,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
    } = this.props;

    const { editing, editIndex } = this.state;
    const { input: _input, updating } = expression;
    const isEditingExpr = editing && editIndex === index;
    if (isEditingExpr || (isEditingExpr && expressionError)) {
      return this.renderExpressionEditInput(expression);
    }

    if (updating) {
      return null;
    }

    const { expressionResultGrip, expressionResultFront } =
      getExpressionResultGripAndFront(expression);

    const root = {
      name: expression.input,
      path: _input,
      contents: {
        value: expressionResultGrip,
        front: expressionResultFront,
      },
    };

    return li(
      {
        className: "expression-container",
        key: _input,
        title: expression.input,
      },
      div(
        {
          className: "expression-content",
        },
        React.createElement(ObjectInspector, {
          roots: [root],
          autoExpandDepth: 0,
          disableWrap: true,
          openLink: openLink,
          createElement: this.createElement,
          onDoubleClick: (items, { depth }) => {
            if (depth === 0) {
              this.editExpression(expression, index);
            }
          },
          onDOMNodeClick: grip => openElementInInspector(grip),
          onInspectIconClick: grip => openElementInInspector(grip),
          onDOMNodeMouseOver: grip => highlightDomElement(grip),
          onDOMNodeMouseOut: grip => unHighlightDomElement(grip),
          shouldRenderTooltip: true,
          mayUseCustomFormatter: true,
        }),
        div(
          {
            className: "expression-container__close-btn",
          },
          React.createElement(CloseButton, {
            handleClick: e => this.deleteExpression(e, expression),
            tooltip: L10N.getStr("expressions.remove.tooltip"),
          })
        )
      )
    );
  };

  renderExpressions() {
    const { expressions, showInput } = this.props;
    return React.createElement(
      React.Fragment,
      null,
      ul(
        {
          className: "pane expressions-list",
        },
        expressions.map(this.renderExpression)
      ),
      showInput && this.renderNewExpressionInput()
    );
  }

  renderAutoCompleteMatches() {
    if (!features.autocompleteExpression) {
      return null;
    }
    const { autocompleteMatches } = this.props;
    if (autocompleteMatches) {
      return datalist(
        {
          id: "autocomplete-matches",
        },
        autocompleteMatches.map((match, index) => {
          return option({
            key: index,
            value: match,
          });
        })
      );
    }
    return datalist({
      id: "autocomplete-matches",
    });
  }

  renderNewExpressionInput() {
    const { expressionError } = this.props;
    const { editing, inputValue, focused } = this.state;
    const error = editing === false && expressionError === true;
    const placeholder = error
      ? L10N.getStr("expressions.errorMsg")
      : L10N.getStr("expressions.placeholder");
    return form(
      {
        className: classnames(
          "expression-input-container expression-input-form",
          {
            focused,
            error,
          }
        ),
        onSubmit: this.handleNewSubmit,
      },
      input({
        className: "input-expression",
        type: "text",
        placeholder: placeholder,
        onChange: this.handleChange,
        onBlur: this.hideInput,
        onKeyDown: this.handleKeyDown,
        onFocus: this.onFocus,
        value: !editing ? inputValue : "",
        ref: c => (this._input = c),
        ...(features.autocompleteExpression && {
          list: "autocomplete-matches",
        }),
      }),
      this.renderAutoCompleteMatches(),
      input({
        type: "submit",
        style: {
          display: "none",
        },
      })
    );
  }

  renderExpressionEditInput(expression) {
    const { expressionError } = this.props;
    const { inputValue, editing, focused } = this.state;
    const error = editing === true && expressionError === true;
    return form(
      {
        key: expression.input,
        className: classnames(
          "expression-input-container expression-input-form",
          {
            focused,
            error,
          }
        ),
        onSubmit: e => this.handleExistingSubmit(e, expression),
      },
      input({
        className: classnames("input-expression", {
          error,
        }),
        type: "text",
        onChange: this.handleChange,
        onBlur: this.clear,
        onKeyDown: this.handleKeyDown,
        onFocus: this.onFocus,
        value: editing ? inputValue : expression.input,
        ref: c => (this._input = c),
        ...(features.autocompleteExpression && {
          list: "autocomplete-matches",
        }),
      }),
      this.renderAutoCompleteMatches(),
      input({
        type: "submit",
        style: {
          display: "none",
        },
      })
    );
  }

  render() {
    const { expressions } = this.props;

    return div(
      { className: "pane" },
      this.renderExpressionsNotification(),
      expressions.length === 0
        ? this.renderNewExpressionInput()
        : this.renderExpressions()
    );
  }
}

const mapStateToProps = state => {
  const selectedFrame = getSelectedFrame(state, getCurrentThread(state));
  const selectedSource = getSelectedSource(state);
  const isPaused = getIsCurrentThreadPaused(state);
  const mapScopesEnabled = isMapScopesEnabled(state);
  const expressions = getExpressions(state);

  const selectedSourceIsNonPrettyPrintedOriginal =
    selectedSource?.isOriginal && !selectedSource?.isPrettyPrinted;

  let isOriginalVariableMappingDisabled, isLoadingOriginalVariables;

  if (selectedSourceIsNonPrettyPrintedOriginal) {
    isOriginalVariableMappingDisabled = isPaused && !mapScopesEnabled;
    isLoadingOriginalVariables =
      isPaused &&
      mapScopesEnabled &&
      !expressions.length &&
      !getOriginalFrameScope(state, selectedFrame)?.scope;
  }

  return {
    isOriginalVariableMappingDisabled,
    isLoadingOriginalVariables,
    autocompleteMatches: getAutocompleteMatchset(state),
    expressions,
    expressionError: getExpressionError(state),
  };
};

export default connect(mapStateToProps, {
  autocomplete: actions.autocomplete,
  clearAutocomplete: actions.clearAutocomplete,
  addExpression: actions.addExpression,
  clearExpressionError: actions.clearExpressionError,
  updateExpression: actions.updateExpression,
  deleteExpression: actions.deleteExpression,
  openLink: actions.openLink,
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
})(Expressions);
