"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _prefs = require("../utils/prefs");

var _actions = require("../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _ShortcutsModal = require("./ShortcutsModal");

var _selectors = require("../selectors/index");

var _devtoolsModules = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-modules"];

var _devtoolsSplitter = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-splitter"];

var _devtoolsSplitter2 = _interopRequireDefault(_devtoolsSplitter);

var _ProjectSearch = require("./ProjectSearch");

var _ProjectSearch2 = _interopRequireDefault(_ProjectSearch);

var _PrimaryPanes = require("./PrimaryPanes/index");

var _PrimaryPanes2 = _interopRequireDefault(_PrimaryPanes);

var _Editor = require("./Editor/index");

var _Editor2 = _interopRequireDefault(_Editor);

var _SecondaryPanes = require("./SecondaryPanes/index");

var _SecondaryPanes2 = _interopRequireDefault(_SecondaryPanes);

var _WelcomeBox = require("./WelcomeBox");

var _WelcomeBox2 = _interopRequireDefault(_WelcomeBox);

var _Tabs = require("./Editor/Tabs");

var _Tabs2 = _interopRequireDefault(_Tabs);

var _QuickOpenModal = require("./QuickOpenModal");

var _QuickOpenModal2 = _interopRequireDefault(_QuickOpenModal);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const shortcuts = new _devtoolsModules.KeyShortcuts({
  window
});
const {
  appinfo
} = _devtoolsModules.Services;
const isMacOS = appinfo.OS === "Darwin";
const horizontalLayoutBreakpoint = window.matchMedia("(min-width: 800px)");
const verticalLayoutBreakpoint = window.matchMedia("(min-width: 10px) and (max-width: 800px)");

class App extends _react.Component {
  constructor(props) {
    super(props);

    this.getChildContext = () => {
      return {
        shortcuts
      };
    };

    this.onEscape = (_, e) => {
      const {
        activeSearch,
        closeActiveSearch,
        closeQuickOpen,
        quickOpenEnabled
      } = this.props;

      if (activeSearch) {
        e.preventDefault();
        closeActiveSearch();
      }

      if (quickOpenEnabled) {
        e.preventDefault();
        closeQuickOpen();
      }
    };

    this.onCommandSlash = () => {
      this.toggleShortcutsModal();
    };

    this.toggleQuickOpenModal = (_, e, query) => {
      const {
        quickOpenEnabled,
        openQuickOpen,
        closeQuickOpen
      } = this.props;
      e.preventDefault();
      e.stopPropagation();

      if (quickOpenEnabled === true) {
        closeQuickOpen();
        return;
      }

      if (query != null) {
        openQuickOpen(query);
        return;
      }

      openQuickOpen();
      return;
    };

    this.onLayoutChange = () => {
      this.setOrientation();
    };

    this.renderEditorPane = () => {
      const {
        startPanelCollapsed,
        endPanelCollapsed
      } = this.props;
      const {
        endPanelSize,
        startPanelSize
      } = this.state;
      const horizontal = this.isHorizontal();
      return _react2.default.createElement("div", {
        className: "editor-pane"
      }, _react2.default.createElement("div", {
        className: "editor-container"
      }, _react2.default.createElement(_Tabs2.default, {
        startPanelCollapsed: startPanelCollapsed,
        endPanelCollapsed: endPanelCollapsed,
        horizontal: horizontal,
        startPanelSize: startPanelSize,
        endPanelSize: endPanelSize
      }), _react2.default.createElement(_Editor2.default, {
        horizontal: horizontal,
        startPanelSize: startPanelSize,
        endPanelSize: endPanelSize
      }), !this.props.selectedSource ? _react2.default.createElement(_WelcomeBox2.default, {
        horizontal: horizontal
      }) : null, _react2.default.createElement(_ProjectSearch2.default, null)));
    };

    this.renderLayout = () => {
      const {
        startPanelCollapsed,
        endPanelCollapsed
      } = this.props;
      const horizontal = this.isHorizontal();
      const maxSize = horizontal ? "70%" : "95%";
      const primaryInitialSize = horizontal ? "250px" : "150px";
      return _react2.default.createElement(_devtoolsSplitter2.default, {
        style: {
          width: "100vw"
        },
        initialHeight: 400,
        initialWidth: 300,
        minSize: 30,
        maxSize: maxSize,
        splitterSize: 1,
        vert: horizontal,
        startPanel: _react2.default.createElement(_devtoolsSplitter2.default, {
          style: {
            width: "100vw"
          },
          initialSize: primaryInitialSize,
          minSize: 30,
          maxSize: "85%",
          splitterSize: 1,
          startPanelCollapsed: startPanelCollapsed,
          startPanel: _react2.default.createElement(_PrimaryPanes2.default, {
            horizontal: horizontal
          }),
          endPanel: this.renderEditorPane()
        }),
        endPanelControl: true,
        endPanel: _react2.default.createElement(_SecondaryPanes2.default, {
          horizontal: horizontal,
          toggleShortcutsModal: () => this.toggleShortcutsModal()
        }),
        endPanelCollapsed: endPanelCollapsed
      });
    };

    this.state = {
      shortcutsModalEnabled: false,
      startPanelSize: 0,
      endPanelSize: 0
    };
  }

  componentDidMount() {
    horizontalLayoutBreakpoint.addListener(this.onLayoutChange);
    verticalLayoutBreakpoint.addListener(this.onLayoutChange);
    this.setOrientation();
    shortcuts.on(L10N.getStr("symbolSearch.search.key2"), (_, e) => this.toggleQuickOpenModal(_, e, "@"));
    const searchKeys = [L10N.getStr("sources.search.key2"), L10N.getStr("sources.search.alt.key")];
    searchKeys.forEach(key => shortcuts.on(key, this.toggleQuickOpenModal));
    shortcuts.on(L10N.getStr("gotoLineModal.key2"), (_, e) => this.toggleQuickOpenModal(_, e, ":"));
    shortcuts.on("Escape", this.onEscape);
    shortcuts.on("Cmd+/", this.onCommandSlash);
  }

  componentWillUnmount() {
    horizontalLayoutBreakpoint.removeListener(this.onLayoutChange);
    verticalLayoutBreakpoint.removeListener(this.onLayoutChange);
    shortcuts.off(L10N.getStr("symbolSearch.search.key2"), this.toggleQuickOpenModal);
    const searchKeys = [L10N.getStr("sources.search.key2"), L10N.getStr("sources.search.alt.key")];
    searchKeys.forEach(key => shortcuts.off(key, this.toggleQuickOpenModal));
    shortcuts.off(L10N.getStr("gotoLineModal.key2"), this.toggleQuickOpenModal);
    shortcuts.off("Escape", this.onEscape);
  }

  isHorizontal() {
    return this.props.orientation === "horizontal";
  }

  setOrientation() {
    // If the orientation does not match (if it is not visible) it will
    // not setOrientation, or if it is the same as before, calling
    // setOrientation will not cause a rerender.
    if (horizontalLayoutBreakpoint.matches) {
      this.props.setOrientation("horizontal");
    } else if (verticalLayoutBreakpoint.matches) {
      this.props.setOrientation("vertical");
    }
  }

  toggleShortcutsModal() {
    this.setState(prevState => ({
      shortcutsModalEnabled: !prevState.shortcutsModalEnabled
    }));
  }

  renderShortcutsModal() {
    const additionalClass = isMacOS ? "mac" : "";

    if (!_prefs.features.shortcuts) {
      return;
    }

    return _react2.default.createElement(_ShortcutsModal.ShortcutsModal, {
      additionalClass: additionalClass,
      enabled: this.state.shortcutsModalEnabled,
      handleClose: () => this.toggleShortcutsModal()
    });
  }

  render() {
    const {
      quickOpenEnabled
    } = this.props;
    return _react2.default.createElement("div", {
      className: "debugger"
    }, this.renderLayout(), quickOpenEnabled === true && _react2.default.createElement(_QuickOpenModal2.default, {
      shortcutsModalEnabled: this.state.shortcutsModalEnabled,
      toggleShortcutsModal: () => this.toggleShortcutsModal()
    }), this.renderShortcutsModal());
  }

}

App.childContextTypes = {
  shortcuts: _propTypes2.default.object
};

const mapStateToProps = state => ({
  selectedSource: (0, _selectors.getSelectedSource)(state),
  startPanelCollapsed: (0, _selectors.getPaneCollapse)(state, "start"),
  endPanelCollapsed: (0, _selectors.getPaneCollapse)(state, "end"),
  activeSearch: (0, _selectors.getActiveSearch)(state),
  quickOpenEnabled: (0, _selectors.getQuickOpenEnabled)(state),
  orientation: (0, _selectors.getOrientation)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  setActiveSearch: _actions2.default.setActiveSearch,
  closeActiveSearch: _actions2.default.closeActiveSearch,
  closeProjectSearch: _actions2.default.closeProjectSearch,
  openQuickOpen: _actions2.default.openQuickOpen,
  closeQuickOpen: _actions2.default.closeQuickOpen,
  setOrientation: _actions2.default.setOrientation
})(App);