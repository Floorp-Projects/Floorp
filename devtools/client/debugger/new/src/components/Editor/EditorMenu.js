"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _clipboard = require("../../utils/clipboard");

var _function = require("../../utils/function");

var _ast = require("../../utils/ast");

var _editor = require("../../utils/editor/index");

var _source = require("../../utils/source");

var _selectors = require("../../selectors/index");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getMenuItems(event, {
  addExpression,
  editor,
  evaluateInConsole,
  flashLineRange,
  getFunctionLocation,
  getFunctionText,
  hasPrettyPrint,
  jumpToMappedLocation,
  onGutterContextMenu,
  selectedLocation,
  selectedSource,
  showSource,
  toggleBlackBox
}) {
  // variables
  const hasSourceMap = !!selectedSource.sourceMapURL;
  const isOriginal = (0, _devtoolsSourceMap.isOriginalId)(selectedLocation.sourceId);
  const isPrettyPrinted = (0, _source.isPretty)(selectedSource);
  const isPrettified = isPrettyPrinted || hasPrettyPrint;
  const isMapped = isOriginal || hasSourceMap;
  const {
    line
  } = editor.codeMirror.coordsChar({
    left: event.clientX,
    top: event.clientY
  });
  const selectionText = editor.codeMirror.getSelection().trim();
  const sourceLocation = (0, _editor.getSourceLocationFromMouseEvent)(editor, selectedLocation, event);
  const isTextSelected = editor.codeMirror.somethingSelected(); // localizations

  const blackboxKey = L10N.getStr("sourceFooter.blackbox.accesskey");
  const blackboxLabel = L10N.getStr("sourceFooter.blackbox");
  const unblackboxLabel = L10N.getStr("sourceFooter.unblackbox");
  const toggleBlackBoxLabel = selectedSource.isBlackBoxed ? unblackboxLabel : blackboxLabel;
  const copyFunctionKey = L10N.getStr("copyFunction.accesskey");
  const copyFunctionLabel = L10N.getStr("copyFunction.label");
  const copySourceKey = L10N.getStr("copySource.accesskey");
  const copySourceLabel = L10N.getStr("copySource");
  const copyToClipboardKey = L10N.getStr("copyToClipboard.accesskey");
  const copyToClipboardLabel = L10N.getStr("copyToClipboard.label");
  const copySourceUri2Key = L10N.getStr("copySourceUri2.accesskey");
  const copySourceUri2Label = L10N.getStr("copySourceUri2");
  const evaluateInConsoleLabel = L10N.getStr("evaluateInConsole.label");
  const jumpToMappedLocKey = L10N.getStr("editor.jumpToMappedLocation1.accesskey");
  const jumpToMappedLocLabel = L10N.getFormatStr("editor.jumpToMappedLocation1", isOriginal ? L10N.getStr("generated") : L10N.getStr("original"));
  const revealInTreeKey = L10N.getStr("sourceTabs.revealInTree.accesskey");
  const revealInTreeLabel = L10N.getStr("sourceTabs.revealInTree");
  const watchExpressionKey = L10N.getStr("expressions.accesskey");
  const watchExpressionLabel = L10N.getStr("expressions.label"); // menu items

  const copyToClipboardItem = {
    id: "node-menu-copy-to-clipboard",
    label: copyToClipboardLabel,
    accesskey: copyToClipboardKey,
    disabled: false,
    click: () => (0, _clipboard.copyToTheClipboard)(selectedSource.text)
  };
  const copySourceItem = {
    id: "node-menu-copy-source",
    label: copySourceLabel,
    accesskey: copySourceKey,
    disabled: selectionText.length === 0,
    click: () => (0, _clipboard.copyToTheClipboard)(selectionText)
  };
  const copySourceUri2Item = {
    id: "node-menu-copy-source-url",
    label: copySourceUri2Label,
    accesskey: copySourceUri2Key,
    disabled: false,
    click: () => (0, _clipboard.copyToTheClipboard)((0, _source.getRawSourceURL)(selectedSource.url))
  };
  const sourceId = selectedSource.id;
  const sourceLine = (0, _editor.toSourceLine)(sourceId, line);
  const functionText = getFunctionText(sourceLine);
  const copyFunctionItem = {
    id: "node-menu-copy-function",
    label: copyFunctionLabel,
    accesskey: copyFunctionKey,
    disabled: !functionText,
    click: () => {
      const {
        location: {
          start,
          end
        }
      } = getFunctionLocation(sourceLine);
      flashLineRange({
        start: start.line,
        end: end.line,
        sourceId: selectedLocation.sourceId
      });
      return (0, _clipboard.copyToTheClipboard)(functionText);
    }
  };
  const jumpToMappedLocationItem = {
    id: "node-menu-jump",
    label: jumpToMappedLocLabel,
    accesskey: jumpToMappedLocKey,
    disabled: !isMapped && !isPrettified,
    click: () => jumpToMappedLocation(sourceLocation)
  };
  const showSourceMenuItem = {
    id: "node-menu-show-source",
    label: revealInTreeLabel,
    accesskey: revealInTreeKey,
    disabled: false,
    click: () => showSource(sourceId)
  };
  const blackBoxMenuItem = {
    id: "node-menu-blackbox",
    label: toggleBlackBoxLabel,
    accesskey: blackboxKey,
    disabled: isOriginal || isPrettyPrinted || hasSourceMap,
    click: () => toggleBlackBox(selectedSource)
  };
  const watchExpressionItem = {
    id: "node-menu-add-watch-expression",
    label: watchExpressionLabel,
    accesskey: watchExpressionKey,
    click: () => addExpression(editor.codeMirror.getSelection())
  };
  const evaluateInConsoleItem = {
    id: "node-menu-evaluate-in-console",
    label: evaluateInConsoleLabel,
    click: () => evaluateInConsole(selectionText)
  }; // construct menu

  const menuItems = [copyToClipboardItem, copySourceItem, copySourceUri2Item, copyFunctionItem, {
    type: "separator"
  }, jumpToMappedLocationItem, showSourceMenuItem, blackBoxMenuItem]; // conditionally added items
  // TODO: Find a new way to only add this for mapped sources?

  if (isTextSelected) {
    menuItems.push(watchExpressionItem, evaluateInConsoleItem);
  }

  return menuItems;
}

class EditorMenu extends _react.Component {
  constructor() {
    super();
  }

  shouldComponentUpdate(nextProps) {
    return nextProps.contextMenu.type === "Editor";
  }

  componentWillUpdate(nextProps) {
    // clear the context menu since it is open
    this.props.setContextMenu("", null);
    return this.showMenu(nextProps);
  }

  showMenu(nextProps) {
    const {
      contextMenu,
      ...options
    } = nextProps;
    const {
      event
    } = contextMenu;
    (0, _devtoolsContextmenu.showMenu)(event, getMenuItems(event, options));
  }

  render() {
    return null;
  }

}

const mapStateToProps = state => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const symbols = (0, _selectors.getSymbols)(state, selectedSource);
  return {
    selectedLocation: (0, _selectors.getSelectedLocation)(state),
    selectedSource,
    hasPrettyPrint: !!(0, _selectors.getPrettySource)(state, selectedSource.id),
    contextMenu: (0, _selectors.getContextMenu)(state),
    getFunctionText: line => (0, _function.findFunctionText)(line, selectedSource, symbols),
    getFunctionLocation: line => (0, _ast.findClosestFunction)(symbols, {
      line,
      column: Infinity
    })
  };
};

const {
  addExpression,
  evaluateInConsole,
  flashLineRange,
  jumpToMappedLocation,
  setContextMenu,
  showSource,
  toggleBlackBox
} = _actions2.default;
const mapDispatchToProps = {
  addExpression,
  evaluateInConsole,
  flashLineRange,
  jumpToMappedLocation,
  setContextMenu,
  showSource,
  toggleBlackBox
};
exports.default = (0, _reactRedux.connect)(mapStateToProps, mapDispatchToProps)(EditorMenu);