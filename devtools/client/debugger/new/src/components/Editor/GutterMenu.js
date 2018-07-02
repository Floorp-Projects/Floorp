"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.gutterMenu = gutterMenu;

var _react = require("devtools/client/shared/vendor/react");

var _devtoolsContextmenu = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-contextmenu"];

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _editor = require("../../utils/editor/index");

var _selectors = require("../../selectors/index");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectWithoutProperties(source, excluded) { if (source == null) return {}; var target = {}; var sourceKeys = Object.keys(source); var key, i; for (i = 0; i < sourceKeys.length; i++) { key = sourceKeys[i]; if (excluded.indexOf(key) >= 0) continue; target[key] = source[key]; } if (Object.getOwnPropertySymbols) { var sourceSymbolKeys = Object.getOwnPropertySymbols(source); for (i = 0; i < sourceSymbolKeys.length; i++) { key = sourceSymbolKeys[i]; if (excluded.indexOf(key) >= 0) continue; if (!Object.prototype.propertyIsEnumerable.call(source, key)) continue; target[key] = source[key]; } } return target; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function gutterMenu({
  breakpoint,
  line,
  event,
  isPaused,
  toggleBreakpoint,
  openConditionalPanel,
  toggleDisabledBreakpoint,
  isCbPanelOpen,
  closeConditionalPanel,
  continueToHere
}) {
  event.stopPropagation();
  event.preventDefault();
  const gutterItems = {
    addBreakpoint: {
      id: "node-menu-add-breakpoint",
      label: L10N.getStr("editor.addBreakpoint")
    },
    addConditional: {
      id: "node-menu-add-conditional-breakpoint",
      label: L10N.getStr("editor.addConditionalBreakpoint")
    },
    removeBreakpoint: {
      id: "node-menu-remove-breakpoint",
      label: L10N.getStr("editor.removeBreakpoint")
    },
    editConditional: {
      id: "node-menu-edit-conditional-breakpoint",
      label: L10N.getStr("editor.editBreakpoint")
    },
    enableBreakpoint: {
      id: "node-menu-enable-breakpoint",
      label: L10N.getStr("editor.enableBreakpoint")
    },
    disableBreakpoint: {
      id: "node-menu-disable-breakpoint",
      label: L10N.getStr("editor.disableBreakpoint")
    },
    continueToHere: {
      id: "node-menu-continue-to-here",
      label: L10N.getStr("editor.continueToHere.label")
    }
  };

  const toggleBreakpointItem = _objectSpread({
    accesskey: L10N.getStr("shortcuts.toggleBreakpoint.accesskey"),
    disabled: false,
    click: () => {
      toggleBreakpoint(line);

      if (isCbPanelOpen) {
        closeConditionalPanel();
      }
    }
  }, breakpoint ? gutterItems.removeBreakpoint : gutterItems.addBreakpoint);

  const conditionalBreakpoint = _objectSpread({
    accesskey: L10N.getStr("editor.addConditionalBreakpoint.accesskey"),
    disabled: false,
    click: () => openConditionalPanel(line)
  }, breakpoint && breakpoint.condition ? gutterItems.editConditional : gutterItems.addConditional);

  const items = [toggleBreakpointItem, conditionalBreakpoint];

  if (isPaused) {
    const continueToHereItem = _objectSpread({
      accesskey: L10N.getStr("editor.continueToHere.accesskey"),
      disabled: false,
      click: () => continueToHere(line)
    }, gutterItems.continueToHere);

    items.push(continueToHereItem);
  }

  if (breakpoint) {
    const disableBreakpoint = _objectSpread({
      accesskey: L10N.getStr("editor.disableBreakpoint.accesskey"),
      disabled: false,
      click: () => toggleDisabledBreakpoint(line)
    }, breakpoint.disabled ? gutterItems.enableBreakpoint : gutterItems.disableBreakpoint);

    items.push(disableBreakpoint);
  }

  (0, _devtoolsContextmenu.showMenu)(event, items);
}

class GutterContextMenuComponent extends _react.Component {
  constructor() {
    super();
  }

  shouldComponentUpdate(nextProps) {
    return nextProps.contextMenu.type === "Gutter";
  }

  componentWillUpdate(nextProps) {
    // clear the context menu since it is open
    this.props.setContextMenu("", null);
    return this.showMenu(nextProps);
  }

  showMenu(nextProps) {
    const {
      contextMenu
    } = nextProps,
          props = _objectWithoutProperties(nextProps, ["contextMenu"]);

    const {
      event
    } = contextMenu;
    const sourceId = props.selectedSource ? props.selectedSource.get("id") : "";
    const line = (0, _editor.lineAtHeight)(props.editor, sourceId, event);
    const breakpoint = nextProps.breakpoints.find(bp => bp.location.line === line);

    if (props.emptyLines && props.emptyLines.includes(line)) {
      return;
    }

    gutterMenu(_objectSpread({
      event,
      sourceId,
      line,
      breakpoint
    }, props));
  }

  render() {
    return null;
  }

}

const mapStateToProps = state => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  return {
    selectedLocation: (0, _selectors.getSelectedLocation)(state),
    selectedSource: selectedSource,
    breakpoints: (0, _selectors.getVisibleBreakpoints)(state),
    isPaused: (0, _selectors.isPaused)(state),
    contextMenu: (0, _selectors.getContextMenu)(state),
    emptyLines: (0, _selectors.getEmptyLines)(state, selectedSource.id)
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, _actions2.default)(GutterContextMenuComponent);