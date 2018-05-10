"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Outline = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _clipboard = require("../../utils/clipboard");

var _function = require("../../utils/function");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

var _PreviewFunction = require("../shared/PreviewFunction");

var _PreviewFunction2 = _interopRequireDefault(_PreviewFunction);

var _lodash = require("devtools/client/shared/vendor/lodash");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Outline extends _react.Component {
  selectItem(location) {
    const {
      selectedSource,
      selectLocation
    } = this.props;

    if (!selectedSource) {
      return;
    }

    const selectedSourceId = selectedSource.get("id");
    const startLine = location.start.line;
    selectLocation({
      sourceId: selectedSourceId,
      line: startLine
    });
  }

  onContextMenu(event, func) {
    event.stopPropagation();
    event.preventDefault();
    const {
      selectedSource,
      getFunctionText,
      flashLineRange,
      selectedLocation
    } = this.props;
    const copyFunctionKey = L10N.getStr("copyFunction.accesskey");
    const copyFunctionLabel = L10N.getStr("copyFunction.label");

    if (!selectedSource) {
      return;
    }

    const sourceLine = func.location.start.line;
    const functionText = getFunctionText(sourceLine);
    const copyFunctionItem = {
      id: "node-menu-copy-function",
      label: copyFunctionLabel,
      accesskey: copyFunctionKey,
      disabled: !functionText,
      click: () => {
        flashLineRange({
          start: func.location.start.line,
          end: func.location.end.line,
          sourceId: selectedLocation.sourceId
        });
        return (0, _clipboard.copyToTheClipboard)(functionText);
      }
    };
    const menuOptions = [copyFunctionItem];
    (0, _devtoolsContextmenu.showMenu)(event, menuOptions);
  }

  renderPlaceholder() {
    const placeholderMessage = this.props.selectedSource ? L10N.getStr("outline.noFunctions") : L10N.getStr("outline.noFileSelected");
    return _react2.default.createElement("div", {
      className: "outline-pane-info"
    }, placeholderMessage);
  }

  renderLoading() {
    return _react2.default.createElement("div", {
      className: "outline-pane-info"
    }, L10N.getStr("loadingText"));
  }

  renderFunction(func) {
    const {
      name,
      location,
      parameterNames
    } = func;
    return _react2.default.createElement("li", {
      key: `${name}:${location.start.line}:${location.start.column}`,
      className: "outline-list__element",
      onClick: () => this.selectItem(location),
      onContextMenu: e => this.onContextMenu(e, func)
    }, _react2.default.createElement("span", {
      className: "outline-list__element-icon"
    }, "\u03BB"), _react2.default.createElement(_PreviewFunction2.default, {
      func: {
        name,
        parameterNames
      }
    }));
  }

  renderClassFunctions(klass, functions) {
    if (klass == null || functions.length == 0) {
      return null;
    }

    const classFunc = functions.find(func => func.name === klass);
    const classFunctions = functions.filter(func => func.klass === klass);
    const classInfo = this.props.symbols.classes.find(c => c.name === klass);
    const heading = classFunc ? _react2.default.createElement("h2", null, this.renderFunction(classFunc)) : _react2.default.createElement("h2", {
      onClick: classInfo ? () => this.selectItem(classInfo.location) : null
    }, _react2.default.createElement("span", {
      className: "keyword"
    }, "class"), " ", klass);
    return _react2.default.createElement("div", {
      className: "outline-list__class",
      key: klass
    }, heading, _react2.default.createElement("ul", {
      className: "outline-list__class-list"
    }, classFunctions.map(func => this.renderFunction(func))));
  }

  renderFunctions(functions) {
    let classes = (0, _lodash.uniq)(functions.map(func => func.klass));
    let namedFunctions = functions.filter(func => func.name != "anonymous" && !func.klass && !classes.includes(func.name));
    let classFunctions = functions.filter(func => func.name != "anonymous" && !!func.klass);

    if (this.props.alphabetizeOutline) {
      namedFunctions = (0, _lodash.sortBy)(namedFunctions, "name");
      classes = (0, _lodash.sortBy)(classes, "klass");
      classFunctions = (0, _lodash.sortBy)(classFunctions, "name");
    }

    return _react2.default.createElement("div", null, _react2.default.createElement("ul", {
      className: "outline-list"
    }, namedFunctions.map(func => this.renderFunction(func)), classes.map(klass => this.renderClassFunctions(klass, classFunctions))), _react2.default.createElement("div", {
      className: "outline-footer bottom"
    }, _react2.default.createElement("button", {
      onClick: () => {
        this.props.onAlphabetizeClick();
      },
      className: this.props.alphabetizeOutline ? "active" : ""
    }, L10N.getStr("outline.sortLabel"))));
  }

  render() {
    const {
      symbols,
      selectedSource
    } = this.props;

    if (!selectedSource) {
      return this.renderPlaceholder();
    }

    if (!symbols || symbols.loading) {
      return this.renderLoading();
    }

    const symbolsToDisplay = symbols.functions.filter(func => func.name != "anonymous");
    return _react2.default.createElement("div", {
      className: "outline"
    }, symbolsToDisplay.length > 0 ? this.renderFunctions(symbols.functions) : this.renderPlaceholder());
  }

}

exports.Outline = Outline;

const mapStateToProps = state => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const symbols = (0, _selectors.getSymbols)(state, selectedSource);
  return {
    symbols,
    selectedSource,
    selectedLocation: (0, _selectors.getSelectedLocation)(state),
    getFunctionText: line => (0, _function.findFunctionText)(line, selectedSource, symbols)
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(Outline);