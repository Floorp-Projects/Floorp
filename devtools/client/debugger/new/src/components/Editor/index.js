"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _propTypes = require("devtools/client/shared/vendor/react-prop-types");

var _propTypes2 = _interopRequireDefault(_propTypes);

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactDom = require("devtools/client/shared/vendor/react-dom");

var _reactDom2 = _interopRequireDefault(_reactDom);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _source = require("../../utils/source");

var _devtoolsEnvironment = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

var _prefs = require("../../utils/prefs");

var _indentation = require("../../utils/indentation");

var _selectors = require("../../selectors/index");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _Footer = require("./Footer");

var _Footer2 = _interopRequireDefault(_Footer);

var _SearchBar = require("./SearchBar");

var _SearchBar2 = _interopRequireDefault(_SearchBar);

var _HighlightLines = require("./HighlightLines");

var _HighlightLines2 = _interopRequireDefault(_HighlightLines);

var _Preview = require("./Preview/index");

var _Preview2 = _interopRequireDefault(_Preview);

var _Breakpoints = require("./Breakpoints");

var _Breakpoints2 = _interopRequireDefault(_Breakpoints);

var _HitMarker = require("./HitMarker");

var _HitMarker2 = _interopRequireDefault(_HitMarker);

var _CallSites = require("./CallSites");

var _CallSites2 = _interopRequireDefault(_CallSites);

var _DebugLine = require("./DebugLine");

var _DebugLine2 = _interopRequireDefault(_DebugLine);

var _HighlightLine = require("./HighlightLine");

var _HighlightLine2 = _interopRequireDefault(_HighlightLine);

var _EmptyLines = require("./EmptyLines");

var _EmptyLines2 = _interopRequireDefault(_EmptyLines);

var _GutterMenu = require("./GutterMenu");

var _GutterMenu2 = _interopRequireDefault(_GutterMenu);

var _EditorMenu = require("./EditorMenu");

var _EditorMenu2 = _interopRequireDefault(_EditorMenu);

var _ConditionalPanel = require("./ConditionalPanel");

var _ConditionalPanel2 = _interopRequireDefault(_ConditionalPanel);

var _editor = require("../../utils/editor/index");

var _ui = require("../../utils/ui");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
// Redux actions
const cssVars = {
  searchbarHeight: "var(--editor-searchbar-height)",
  secondSearchbarHeight: "var(--editor-second-searchbar-height)",
  footerHeight: "var(--editor-footer-height)"
};

class Editor extends _react.PureComponent {
  constructor(props) {
    super(props);

    this.onToggleBreakpoint = (key, e) => {
      e.preventDefault();
      e.stopPropagation();
      const {
        selectedSource,
        conditionalPanelLine
      } = this.props;

      if (!selectedSource) {
        return;
      }

      const line = this.getCurrentLine();

      if (e.shiftKey) {
        this.toggleConditionalPanel(line);
      } else if (!conditionalPanelLine) {
        this.props.toggleBreakpoint(line);
      } else {
        this.toggleConditionalPanel(line);
        this.props.toggleBreakpoint(line);
      }
    };

    this.onToggleConditionalPanel = (key, e) => {
      e.stopPropagation();
      e.preventDefault();
      const line = this.getCurrentLine();
      this.toggleConditionalPanel(line);
    };

    this.onEscape = (key, e) => {
      if (!this.state.editor) {
        return;
      }

      const {
        codeMirror
      } = this.state.editor;

      if (codeMirror.listSelections().length > 1) {
        codeMirror.execCommand("singleSelection");
        e.preventDefault();
      }
    };

    this.onSearchAgain = (_, e) => {
      this.props.traverseResults(e.shiftKey, this.state.editor);
    };

    this.onGutterClick = (cm, line, gutter, ev) => {
      const {
        selectedSource,
        conditionalPanelLine,
        closeConditionalPanel,
        addOrToggleDisabledBreakpoint,
        toggleBreakpoint,
        continueToHere
      } = this.props; // ignore right clicks in the gutter

      if (ev.ctrlKey && ev.button === 0 || ev.which === 3 || selectedSource && selectedSource.isBlackBoxed || !selectedSource) {
        return;
      }

      if (conditionalPanelLine) {
        return closeConditionalPanel();
      }

      if (gutter === "CodeMirror-foldgutter") {
        return;
      }

      const sourceLine = (0, _editor.toSourceLine)(selectedSource.id, line);

      if (ev.altKey) {
        return continueToHere(sourceLine);
      }

      if (ev.shiftKey) {
        return addOrToggleDisabledBreakpoint(sourceLine);
      }

      return toggleBreakpoint(sourceLine);
    };

    this.onGutterContextMenu = event => {
      event.stopPropagation();
      event.preventDefault();
      return this.props.setContextMenu("Gutter", event);
    };

    this.toggleConditionalPanel = line => {
      const {
        conditionalPanelLine,
        closeConditionalPanel,
        openConditionalPanel
      } = this.props;

      if (conditionalPanelLine) {
        return closeConditionalPanel();
      }

      return openConditionalPanel(line);
    };

    this.closeConditionalPanel = () => {
      return this.props.closeConditionalPanel();
    };

    this.state = {
      highlightedLineRange: null,
      editor: null
    };
  }

  componentWillReceiveProps(nextProps) {
    if (!this.state.editor) {
      return;
    }

    (0, _ui.resizeBreakpointGutter)(this.state.editor.codeMirror);
    (0, _ui.resizeToggleButton)(this.state.editor.codeMirror);
  }

  componentWillUpdate(nextProps) {
    this.setText(nextProps);
    this.setSize(nextProps);
    this.scrollToLocation(nextProps);
  }

  setupEditor() {
    const editor = (0, _editor.getEditor)(); // disables the default search shortcuts
    // $FlowIgnore

    editor._initShortcuts = () => {};

    const node = _reactDom2.default.findDOMNode(this);

    if (node instanceof HTMLElement) {
      editor.appendToLocalElement(node.querySelector(".editor-mount"));
    }

    const {
      codeMirror
    } = editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();
    (0, _ui.resizeBreakpointGutter)(codeMirror);
    (0, _ui.resizeToggleButton)(codeMirror);
    codeMirror.on("gutterClick", this.onGutterClick); // Set code editor wrapper to be focusable

    codeMirrorWrapper.tabIndex = 0;
    codeMirrorWrapper.addEventListener("keydown", e => this.onKeyDown(e));
    codeMirrorWrapper.addEventListener("click", e => this.onClick(e));
    codeMirrorWrapper.addEventListener("mouseover", (0, _editor.onMouseOver)(codeMirror));

    const toggleFoldMarkerVisibility = e => {
      if (node instanceof HTMLElement) {
        node.querySelectorAll(".CodeMirror-guttermarker-subtle").forEach(elem => {
          elem.classList.toggle("visible");
        });
      }
    };

    const codeMirrorGutter = codeMirror.getGutterElement();
    codeMirrorGutter.addEventListener("mouseleave", toggleFoldMarkerVisibility);
    codeMirrorGutter.addEventListener("mouseenter", toggleFoldMarkerVisibility);

    if (!(0, _devtoolsEnvironment.isFirefox)()) {
      codeMirror.on("gutterContextMenu", (cm, line, eventName, event) => this.onGutterContextMenu(event));
      codeMirror.on("contextmenu", (cm, event) => this.openMenu(event));
    } else {
      codeMirrorWrapper.addEventListener("contextmenu", event => this.openMenu(event));
    }

    this.setState({
      editor
    });
    return editor;
  }

  componentDidMount() {
    const {
      shortcuts
    } = this.context;
    const searchAgainKey = L10N.getStr("sourceSearch.search.again.key2");
    const searchAgainPrevKey = L10N.getStr("sourceSearch.search.againPrev.key2");
    shortcuts.on(L10N.getStr("toggleBreakpoint.key"), this.onToggleBreakpoint);
    shortcuts.on(L10N.getStr("toggleCondPanel.key"), this.onToggleConditionalPanel);
    shortcuts.on("Esc", this.onEscape);
    shortcuts.on(searchAgainPrevKey, this.onSearchAgain);
    shortcuts.on(searchAgainKey, this.onSearchAgain);
  }

  componentWillUnmount() {
    if (this.state.editor) {
      this.state.editor.destroy();
      this.setState({
        editor: null
      });
    }

    const searchAgainKey = L10N.getStr("sourceSearch.search.again.key2");
    const searchAgainPrevKey = L10N.getStr("sourceSearch.search.againPrev.key2");
    const shortcuts = this.context.shortcuts;
    shortcuts.off(L10N.getStr("toggleBreakpoint.key"));
    shortcuts.off(L10N.getStr("toggleCondPanel.key"));
    shortcuts.off(searchAgainPrevKey);
    shortcuts.off(searchAgainKey);
  }

  componentDidUpdate(prevProps, prevState) {
    const {
      selectedSource
    } = this.props; // NOTE: when devtools are opened, the editor is not set when
    // the source loads so we need to wait until the editor is
    // set to update the text and size.

    if (!prevState.editor && selectedSource) {
      if (!this.state.editor) {
        const editor = this.setupEditor();
        (0, _editor.updateDocument)(editor, selectedSource);
      } else {
        this.setText(this.props);
        this.setSize(this.props);
      }
    }
  }

  getCurrentLine() {
    const {
      codeMirror
    } = this.state.editor;
    const {
      selectedSource
    } = this.props;
    const line = (0, _editor.getCursorLine)(codeMirror);
    return (0, _editor.toSourceLine)(selectedSource.id, line);
  }

  onKeyDown(e) {
    const {
      codeMirror
    } = this.state.editor;
    const {
      key,
      target
    } = e;
    const codeWrapper = codeMirror.getWrapperElement();
    const textArea = codeWrapper.querySelector("textArea");

    if (key === "Escape" && target == textArea) {
      e.stopPropagation();
      e.preventDefault();
      codeWrapper.focus();
    } else if (key === "Enter" && target == codeWrapper) {
      e.preventDefault(); // Focus into editor's text area

      textArea.focus();
    }
  }
  /*
   * The default Esc command is overridden in the CodeMirror keymap to allow
   * the Esc keypress event to be catched by the toolbox and trigger the
   * split console. Restore it here, but preventDefault if and only if there
   * is a multiselection.
   */


  openMenu(event) {
    event.stopPropagation();
    event.preventDefault();
    const {
      setContextMenu
    } = this.props;

    if (event.target.classList.contains("CodeMirror-linenumber")) {
      return setContextMenu("Gutter", event);
    }

    return setContextMenu("Editor", event);
  }

  onClick(e) {
    const {
      selectedLocation,
      jumpToMappedLocation
    } = this.props;

    if (e.metaKey && e.altKey) {
      const sourceLocation = (0, _editor.getSourceLocationFromMouseEvent)(this.state.editor, selectedLocation, e);
      jumpToMappedLocation(sourceLocation);
    }
  }

  shouldScrollToLocation(nextProps) {
    const {
      selectedLocation,
      selectedSource
    } = this.props;
    const {
      editor
    } = this.state;

    if (!editor || !nextProps.selectedSource || !nextProps.selectedLocation || !(0, _source.isLoaded)(nextProps.selectedSource)) {
      return false;
    }

    const isFirstLoad = (!selectedSource || !(0, _source.isLoaded)(selectedSource)) && (0, _source.isLoaded)(nextProps.selectedSource);
    const locationChanged = selectedLocation !== nextProps.selectedLocation;
    return isFirstLoad || locationChanged;
  }

  scrollToLocation(nextProps) {
    const {
      editor
    } = this.state;

    if (this.shouldScrollToLocation(nextProps)) {
      let {
        line,
        column
      } = (0, _editor.toEditorPosition)(nextProps.selectedLocation);

      if ((0, _editor.hasDocument)(nextProps.selectedSource.id)) {
        const doc = (0, _editor.getDocument)(nextProps.selectedSource.id);
        const lineText = doc.getLine(line);
        column = Math.max(column, (0, _indentation.getIndentation)(lineText));
      }

      (0, _editor.scrollToColumn)(editor.codeMirror, line, column);
    }
  }

  setSize(nextProps) {
    if (!this.state.editor) {
      return;
    }

    if (nextProps.startPanelSize !== this.props.startPanelSize || nextProps.endPanelSize !== this.props.endPanelSize) {
      this.state.editor.codeMirror.setSize();
    }
  }

  setText(props) {
    const {
      selectedSource,
      symbols
    } = props;

    if (!this.state.editor) {
      return;
    } // check if we previously had a selected source


    if (!selectedSource) {
      return this.clearEditor();
    }

    if (!(0, _source.isLoaded)(selectedSource)) {
      return (0, _editor.showLoading)(this.state.editor);
    }

    if (selectedSource.error) {
      return this.showErrorMessage(selectedSource.error);
    }

    if (selectedSource) {
      return (0, _editor.showSourceText)(this.state.editor, selectedSource, symbols);
    }
  }

  clearEditor() {
    const {
      editor
    } = this.state;

    if (!editor) {
      return;
    }

    (0, _editor.clearEditor)(editor);
  }

  showErrorMessage(msg) {
    const {
      editor
    } = this.state;

    if (!editor) {
      return;
    }

    (0, _editor.showErrorMessage)(editor, msg);
  }

  getInlineEditorStyles() {
    const {
      selectedSource,
      horizontal,
      searchOn
    } = this.props;
    const subtractions = [];

    if ((0, _editor.shouldShowFooter)(selectedSource, horizontal)) {
      subtractions.push(cssVars.footerHeight);
    }

    if (searchOn) {
      subtractions.push(cssVars.searchbarHeight);
      subtractions.push(cssVars.secondSearchbarHeight);
    }

    return {
      height: subtractions.length === 0 ? "100%" : `calc(100% - ${subtractions.join(" - ")})`
    };
  }

  renderHitCounts() {
    const {
      hitCount,
      selectedSource
    } = this.props;

    if (!selectedSource || !(0, _source.isLoaded)(selectedSource) || !hitCount || !this.state.editor) {
      return;
    }

    return hitCount.filter(marker => marker.get("count") > 0).map(marker => _react2.default.createElement(_HitMarker2.default, {
      key: marker.get("line"),
      hitData: marker.toJS(),
      editor: this.state.editor.codeMirror
    }));
  }

  renderItems() {
    const {
      horizontal,
      selectedSource
    } = this.props;
    const {
      editor
    } = this.state;

    if (!editor || !selectedSource) {
      return null;
    }

    return _react2.default.createElement("div", null, _react2.default.createElement(_DebugLine2.default, null), _react2.default.createElement(_HighlightLine2.default, null), _react2.default.createElement(_EmptyLines2.default, {
      editor: editor
    }), _react2.default.createElement(_Breakpoints2.default, {
      editor: editor
    }), _react2.default.createElement(_Preview2.default, {
      editor: editor,
      editorRef: this.$editorWrapper
    }), ";", _react2.default.createElement(_Footer2.default, {
      editor: editor,
      horizontal: horizontal
    }), _react2.default.createElement(_HighlightLines2.default, {
      editor: editor
    }), _react2.default.createElement(_EditorMenu2.default, {
      editor: editor
    }), _react2.default.createElement(_GutterMenu2.default, {
      editor: editor
    }), _react2.default.createElement(_ConditionalPanel2.default, {
      editor: editor
    }), _prefs.features.columnBreakpoints ? _react2.default.createElement(_CallSites2.default, {
      editor: editor
    }) : null, this.renderHitCounts());
  }

  renderSearchBar() {
    const {
      editor
    } = this.state;

    if (!editor) {
      return null;
    }

    return _react2.default.createElement(_SearchBar2.default, {
      editor: editor
    });
  }

  render() {
    const {
      coverageOn
    } = this.props;
    return _react2.default.createElement("div", {
      className: (0, _classnames2.default)("editor-wrapper", {
        "coverage-on": coverageOn
      }),
      ref: c => this.$editorWrapper = c
    }, this.renderSearchBar(), _react2.default.createElement("div", {
      className: "editor-mount devtools-monospace",
      style: this.getInlineEditorStyles()
    }), this.renderItems());
  }

}

Editor.contextTypes = {
  shortcuts: _propTypes2.default.object
};

const mapStateToProps = state => {
  const selectedSource = (0, _selectors.getSelectedSource)(state);
  const sourceId = selectedSource ? selectedSource.id : "";
  return {
    selectedLocation: (0, _selectors.getSelectedLocation)(state),
    selectedSource,
    searchOn: (0, _selectors.getActiveSearch)(state) === "file",
    hitCount: (0, _selectors.getHitCountForSource)(state, sourceId),
    coverageOn: (0, _selectors.getCoverageEnabled)(state),
    conditionalPanelLine: (0, _selectors.getConditionalPanelLine)(state),
    symbols: (0, _selectors.getSymbols)(state, selectedSource)
  };
};

exports.default = (0, _reactRedux.connect)(mapStateToProps, {
  openConditionalPanel: _actions2.default.openConditionalPanel,
  closeConditionalPanel: _actions2.default.closeConditionalPanel,
  setContextMenu: _actions2.default.setContextMenu,
  continueToHere: _actions2.default.continueToHere,
  toggleBreakpoint: _actions2.default.toggleBreakpoint,
  addOrToggleDisabledBreakpoint: _actions2.default.addOrToggleDisabledBreakpoint,
  jumpToMappedLocation: _actions2.default.jumpToMappedLocation,
  traverseResults: _actions2.default.traverseResults
})(Editor);