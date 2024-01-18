/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import React, { PureComponent } from "devtools/client/shared/vendor/react";
import { div } from "devtools/client/shared/vendor/react-dom-factories";
import { bindActionCreators } from "devtools/client/shared/vendor/redux";
import ReactDOM from "devtools/client/shared/vendor/react-dom";
import { connect } from "devtools/client/shared/vendor/react-redux";

import { getLineText, isLineBlackboxed } from "./../../utils/source";
import { createLocation } from "./../../utils/location";
import { getIndentation } from "../../utils/indentation";

import {
  getActiveSearch,
  getSelectedLocation,
  getSelectedSource,
  getSelectedSourceTextContent,
  getSelectedBreakableLines,
  getConditionalPanelLocation,
  getSymbols,
  getIsCurrentThreadPaused,
  getSkipPausing,
  getInlinePreview,
  getBlackBoxRanges,
  isSourceBlackBoxed,
  getHighlightedLineRangeForSelectedSource,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
  isMapScopesEnabled,
} from "../../selectors/index";

// Redux actions
import actions from "../../actions/index";

import SearchInFileBar from "./SearchInFileBar";
import HighlightLines from "./HighlightLines";
import Preview from "./Preview/index";
import Breakpoints from "./Breakpoints";
import ColumnBreakpoints from "./ColumnBreakpoints";
import DebugLine from "./DebugLine";
import HighlightLine from "./HighlightLine";
import EmptyLines from "./EmptyLines";
import ConditionalPanel from "./ConditionalPanel";
import InlinePreviews from "./InlinePreviews";
import Exceptions from "./Exceptions";
import BlackboxLines from "./BlackboxLines";

import {
  showSourceText,
  showLoading,
  showErrorMessage,
  getEditor,
  clearEditor,
  getCursorLine,
  getCursorColumn,
  lineAtHeight,
  toSourceLine,
  getDocument,
  scrollToPosition,
  toEditorPosition,
  getSourceLocationFromMouseEvent,
  hasDocument,
  onMouseOver,
  startOperation,
  endOperation,
} from "../../utils/editor/index";

import { resizeToggleButton, resizeBreakpointGutter } from "../../utils/ui";

const { debounce } = require("resource://devtools/shared/debounce.js");
const classnames = require("resource://devtools/client/shared/classnames.js");

const { appinfo } = Services;
const isMacOS = appinfo.OS === "Darwin";

function isSecondary(ev) {
  return isMacOS && ev.ctrlKey && ev.button === 0;
}

function isCmd(ev) {
  return isMacOS ? ev.metaKey : ev.ctrlKey;
}

const cssVars = {
  searchbarHeight: "var(--editor-searchbar-height)",
};

class Editor extends PureComponent {
  static get propTypes() {
    return {
      selectedSource: PropTypes.object,
      selectedSourceTextContent: PropTypes.object,
      selectedSourceIsBlackBoxed: PropTypes.bool,
      closeTab: PropTypes.func.isRequired,
      toggleBreakpointAtLine: PropTypes.func.isRequired,
      conditionalPanelLocation: PropTypes.object,
      closeConditionalPanel: PropTypes.func.isRequired,
      openConditionalPanel: PropTypes.func.isRequired,
      updateViewport: PropTypes.func.isRequired,
      isPaused: PropTypes.bool.isRequired,
      addBreakpointAtLine: PropTypes.func.isRequired,
      continueToHere: PropTypes.func.isRequired,
      updateCursorPosition: PropTypes.func.isRequired,
      jumpToMappedLocation: PropTypes.func.isRequired,
      selectedLocation: PropTypes.object,
      symbols: PropTypes.object,
      startPanelSize: PropTypes.number.isRequired,
      endPanelSize: PropTypes.number.isRequired,
      searchInFileEnabled: PropTypes.bool.isRequired,
      inlinePreviewEnabled: PropTypes.bool.isRequired,
      skipPausing: PropTypes.bool.isRequired,
      blackboxedRanges: PropTypes.object.isRequired,
      breakableLines: PropTypes.object.isRequired,
      highlightedLineRange: PropTypes.object,
      isSourceOnIgnoreList: PropTypes.bool,
      mapScopesEnabled: PropTypes.bool,
    };
  }

  $editorWrapper;
  constructor(props) {
    super(props);

    this.state = {
      editor: null,
    };
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    let { editor } = this.state;

    if (!editor && nextProps.selectedSource) {
      editor = this.setupEditor();
    }

    const shouldUpdateText =
      nextProps.selectedSource !== this.props.selectedSource ||
      nextProps.selectedSourceTextContent?.value !==
        this.props.selectedSourceTextContent?.value ||
      nextProps.symbols !== this.props.symbols;

    const shouldUpdateSize =
      nextProps.startPanelSize !== this.props.startPanelSize ||
      nextProps.endPanelSize !== this.props.endPanelSize;

    const shouldScroll =
      nextProps.selectedLocation &&
      this.shouldScrollToLocation(nextProps, editor);

    if (shouldUpdateText || shouldUpdateSize || shouldScroll) {
      startOperation();
      if (shouldUpdateText) {
        this.setText(nextProps, editor);
      }
      if (shouldUpdateSize) {
        editor.codeMirror.setSize();
      }
      if (shouldScroll) {
        this.scrollToLocation(nextProps, editor);
      }
      endOperation();
    }

    if (this.props.selectedSource != nextProps.selectedSource) {
      this.props.updateViewport();
      resizeBreakpointGutter(editor.codeMirror);
      resizeToggleButton(editor.codeMirror);
    }
  }

  setupEditor() {
    const editor = getEditor();

    // disables the default search shortcuts
    editor._initShortcuts = () => {};

    const node = ReactDOM.findDOMNode(this);
    if (node instanceof HTMLElement) {
      editor.appendToLocalElement(node.querySelector(".editor-mount"));
    }

    const { codeMirror } = editor;

    this.abortController = new window.AbortController();

    // CodeMirror refreshes its internal state on window resize, but we need to also
    // refresh it when the side panels are resized.
    // We could have a ResizeObserver instead, but we wouldn't be able to differentiate
    // between window resize and side panel resize and as a result, might refresh
    // codeMirror twice, which is wasteful.
    window.document
      .querySelector(".editor-pane")
      .addEventListener("resizeend", () => codeMirror.refresh(), {
        signal: this.abortController.signal,
      });

    codeMirror.on("gutterClick", this.onGutterClick);
    codeMirror.on("cursorActivity", this.onCursorChange);

    const codeMirrorWrapper = codeMirror.getWrapperElement();
    // Set code editor wrapper to be focusable
    codeMirrorWrapper.tabIndex = 0;
    codeMirrorWrapper.addEventListener("keydown", e => this.onKeyDown(e));
    codeMirrorWrapper.addEventListener("click", e => this.onClick(e));
    codeMirrorWrapper.addEventListener("mouseover", onMouseOver(codeMirror));

    const toggleFoldMarkerVisibility = e => {
      if (node instanceof HTMLElement) {
        node
          .querySelectorAll(".CodeMirror-guttermarker-subtle")
          .forEach(elem => {
            elem.classList.toggle("visible");
          });
      }
    };

    const codeMirrorGutter = codeMirror.getGutterElement();
    codeMirrorGutter.addEventListener("mouseleave", toggleFoldMarkerVisibility);
    codeMirrorGutter.addEventListener("mouseenter", toggleFoldMarkerVisibility);
    codeMirrorWrapper.addEventListener("contextmenu", event =>
      this.openMenu(event)
    );

    codeMirror.on("scroll", this.onEditorScroll);
    this.onEditorScroll();
    this.setState({ editor });
    return editor;
  }

  componentDidMount() {
    const { shortcuts } = this.context;

    shortcuts.on(L10N.getStr("toggleBreakpoint.key"), this.onToggleBreakpoint);
    shortcuts.on(
      L10N.getStr("toggleCondPanel.breakpoint.key"),
      this.onToggleConditionalPanel
    );
    shortcuts.on(
      L10N.getStr("toggleCondPanel.logPoint.key"),
      this.onToggleConditionalPanel
    );
    shortcuts.on(
      L10N.getStr("sourceTabs.closeTab.key"),
      this.onCloseShortcutPress
    );
    shortcuts.on("Esc", this.onEscape);
  }

  onCloseShortcutPress = e => {
    const { selectedSource } = this.props;
    if (selectedSource) {
      e.preventDefault();
      e.stopPropagation();
      this.props.closeTab(selectedSource, "shortcut");
    }
  };

  componentWillUnmount() {
    const { editor } = this.state;
    if (editor) {
      editor.destroy();
      editor.codeMirror.off("scroll", this.onEditorScroll);
      this.setState({ editor: null });
    }

    const { shortcuts } = this.context;
    shortcuts.off(L10N.getStr("sourceTabs.closeTab.key"));
    shortcuts.off(L10N.getStr("toggleBreakpoint.key"));
    shortcuts.off(L10N.getStr("toggleCondPanel.breakpoint.key"));
    shortcuts.off(L10N.getStr("toggleCondPanel.logPoint.key"));

    if (this.abortController) {
      this.abortController.abort();
      this.abortController = null;
    }
  }

  getCurrentLine() {
    const { codeMirror } = this.state.editor;
    const { selectedSource } = this.props;
    if (!selectedSource) {
      return null;
    }

    const line = getCursorLine(codeMirror);
    return toSourceLine(selectedSource.id, line);
  }

  onToggleBreakpoint = e => {
    e.preventDefault();
    e.stopPropagation();

    const line = this.getCurrentLine();
    if (typeof line !== "number") {
      return;
    }

    this.props.toggleBreakpointAtLine(line);
  };

  onToggleConditionalPanel = e => {
    e.stopPropagation();
    e.preventDefault();

    const {
      conditionalPanelLocation,
      closeConditionalPanel,
      openConditionalPanel,
      selectedSource,
    } = this.props;

    const line = this.getCurrentLine();

    const { codeMirror } = this.state.editor;
    // add one to column for correct position in editor.
    const column = getCursorColumn(codeMirror) + 1;

    if (conditionalPanelLocation) {
      return closeConditionalPanel();
    }

    if (!selectedSource || typeof line !== "number") {
      return null;
    }

    return openConditionalPanel(
      createLocation({
        line,
        column,
        source: selectedSource,
      }),
      false
    );
  };

  onEditorScroll = debounce(this.props.updateViewport, 75);

  onKeyDown(e) {
    const { codeMirror } = this.state.editor;
    const { key, target } = e;
    const codeWrapper = codeMirror.getWrapperElement();
    const textArea = codeWrapper.querySelector("textArea");

    if (key === "Escape" && target == textArea) {
      e.stopPropagation();
      e.preventDefault();
      codeWrapper.focus();
    } else if (key === "Enter" && target == codeWrapper) {
      e.preventDefault();
      // Focus into editor's text area
      textArea.focus();
    }
  }

  /*
   * The default Esc command is overridden in the CodeMirror keymap to allow
   * the Esc keypress event to be catched by the toolbox and trigger the
   * split console. Restore it here, but preventDefault if and only if there
   * is a multiselection.
   */
  onEscape = e => {
    if (!this.state.editor) {
      return;
    }

    const { codeMirror } = this.state.editor;
    if (codeMirror.listSelections().length > 1) {
      codeMirror.execCommand("singleSelection");
      e.preventDefault();
    }
  };

  openMenu(event) {
    event.stopPropagation();
    event.preventDefault();

    const {
      selectedSource,
      selectedSourceTextContent,
      conditionalPanelLocation,
      closeConditionalPanel,
    } = this.props;

    const { editor } = this.state;

    if (!selectedSource || !editor) {
      return;
    }

    // only allow one conditionalPanel location.
    if (conditionalPanelLocation) {
      closeConditionalPanel();
    }

    const target = event.target;
    const { id: sourceId } = selectedSource;
    const line = lineAtHeight(editor, sourceId, event);

    if (typeof line != "number") {
      return;
    }

    if (target.classList.contains("CodeMirror-linenumber")) {
      const location = createLocation({
        line,
        column: undefined,
        source: selectedSource,
      });

      const lineText = getLineText(
        sourceId,
        selectedSourceTextContent,
        line
      ).trim();

      this.props.showEditorGutterContextMenu(event, editor, location, lineText);
      return;
    }

    if (target.getAttribute("id") === "columnmarker") {
      return;
    }

    const location = getSourceLocationFromMouseEvent(
      editor,
      selectedSource,
      event
    );

    this.props.showEditorContextMenu(event, editor, location);
  }

  /**
   * CodeMirror event handler, called whenever the cursor moves
   * for user-driven or programatic reasons.
   */
  onCursorChange = event => {
    const { line, ch } = event.doc.getCursor();
    this.props.selectLocation(
      createLocation({
        source: this.props.selectedSource,
        // CodeMirror cursor location is all 0-based.
        // Whereast in DevTools frontend and backend,
        // only colunm is 0-based, the line is 1 based.
        line: line + 1,
        column: ch,
      }),
      {
        // Reset the context, so that we don't switch to original
        // while moving the cursor within a bundle
        keepContext: false,
      }
    );
  };

  onGutterClick = (cm, line, gutter, ev) => {
    const {
      selectedSource,
      conditionalPanelLocation,
      closeConditionalPanel,
      addBreakpointAtLine,
      continueToHere,
      breakableLines,
      blackboxedRanges,
      isSourceOnIgnoreList,
    } = this.props;

    // ignore right clicks in the gutter
    if (isSecondary(ev) || ev.button === 2 || !selectedSource) {
      return;
    }

    if (conditionalPanelLocation) {
      closeConditionalPanel();
      return;
    }

    if (gutter === "CodeMirror-foldgutter") {
      return;
    }

    const sourceLine = toSourceLine(selectedSource.id, line);
    if (typeof sourceLine !== "number") {
      return;
    }

    // ignore clicks on a non-breakable line
    if (!breakableLines.has(sourceLine)) {
      return;
    }

    if (isCmd(ev)) {
      continueToHere(
        createLocation({
          line: sourceLine,
          column: undefined,
          source: selectedSource,
        })
      );
      return;
    }

    addBreakpointAtLine(
      sourceLine,
      ev.altKey,
      ev.shiftKey ||
        isLineBlackboxed(
          blackboxedRanges[selectedSource.url],
          sourceLine,
          isSourceOnIgnoreList
        )
    );
  };

  onGutterContextMenu = event => {
    this.openMenu(event);
  };

  onClick(e) {
    const { selectedSource, updateCursorPosition, jumpToMappedLocation } =
      this.props;

    if (selectedSource) {
      const sourceLocation = getSourceLocationFromMouseEvent(
        this.state.editor,
        selectedSource,
        e
      );

      if (e.metaKey && e.altKey) {
        jumpToMappedLocation(sourceLocation);
      }

      updateCursorPosition(sourceLocation);
    }
  }

  shouldScrollToLocation(nextProps, editor) {
    if (
      !nextProps.selectedLocation?.line ||
      !nextProps.selectedSourceTextContent
    ) {
      return false;
    }

    const { selectedLocation, selectedSourceTextContent } = this.props;
    const contentChanged =
      !selectedSourceTextContent?.value &&
      nextProps.selectedSourceTextContent?.value;
    const locationChanged = selectedLocation !== nextProps.selectedLocation;
    const symbolsChanged = nextProps.symbols != this.props.symbols;

    return contentChanged || locationChanged || symbolsChanged;
  }

  scrollToLocation(nextProps, editor) {
    const { selectedLocation, selectedSource } = nextProps;

    let { line, column } = toEditorPosition(selectedLocation);

    if (selectedSource && hasDocument(selectedSource.id)) {
      const doc = getDocument(selectedSource.id);
      const lineText = doc.getLine(line);
      column = Math.max(column, getIndentation(lineText));
    }

    scrollToPosition(editor.codeMirror, line, column);
  }

  setText(props, editor) {
    const { selectedSource, selectedSourceTextContent, symbols } = props;

    if (!editor) {
      return;
    }

    // check if we previously had a selected source
    if (!selectedSource) {
      this.clearEditor();
      return;
    }

    if (!selectedSourceTextContent?.value) {
      showLoading(editor);
      return;
    }

    if (selectedSourceTextContent.state === "rejected") {
      let { value } = selectedSourceTextContent;
      if (typeof value !== "string") {
        value = "Unexpected source error";
      }

      this.showErrorMessage(value);
      return;
    }

    showSourceText(editor, selectedSource, selectedSourceTextContent, symbols);
  }

  clearEditor() {
    const { editor } = this.state;
    if (!editor) {
      return;
    }

    clearEditor(editor);
  }

  showErrorMessage(msg) {
    const { editor } = this.state;
    if (!editor) {
      return;
    }

    showErrorMessage(editor, msg);
  }

  getInlineEditorStyles() {
    const { searchInFileEnabled } = this.props;

    if (searchInFileEnabled) {
      return {
        height: `calc(100% - ${cssVars.searchbarHeight})`,
      };
    }

    return {
      height: "100%",
    };
  }

  renderItems() {
    const {
      selectedSource,
      conditionalPanelLocation,
      isPaused,
      inlinePreviewEnabled,
      highlightedLineRange,
      blackboxedRanges,
      isSourceOnIgnoreList,
      selectedSourceIsBlackBoxed,
      mapScopesEnabled,
    } = this.props;
    const { editor } = this.state;

    if (!selectedSource || !editor || !getDocument(selectedSource.id)) {
      return null;
    }
    return div(
      null,
      React.createElement(DebugLine, null),
      React.createElement(HighlightLine, null),
      React.createElement(EmptyLines, {
        editor,
      }),
      React.createElement(Breakpoints, {
        editor,
      }),
      isPaused &&
        selectedSource.isOriginal &&
        !selectedSource.isPrettyPrinted &&
        !mapScopesEnabled
        ? null
        : React.createElement(Preview, {
            editor,
            editorRef: this.$editorWrapper,
          }),
      highlightedLineRange
        ? React.createElement(HighlightLines, {
            editor,
            range: highlightedLineRange,
          })
        : null,
      isSourceOnIgnoreList || selectedSourceIsBlackBoxed
        ? React.createElement(BlackboxLines, {
            editor,
            selectedSource,
            isSourceOnIgnoreList,
            blackboxedRangesForSelectedSource:
              blackboxedRanges[selectedSource.url],
          })
        : null,
      React.createElement(Exceptions, null),
      conditionalPanelLocation
        ? React.createElement(ConditionalPanel, {
            editor,
          })
        : null,
      React.createElement(ColumnBreakpoints, {
        editor,
      }),
      isPaused &&
        inlinePreviewEnabled &&
        (!selectedSource.isOriginal ||
          (selectedSource.isOriginal && selectedSource.isPrettyPrinted) ||
          (selectedSource.isOriginal && mapScopesEnabled))
        ? React.createElement(InlinePreviews, {
            editor,
            selectedSource,
          })
        : null
    );
  }

  renderSearchInFileBar() {
    if (!this.props.selectedSource) {
      return null;
    }
    return React.createElement(SearchInFileBar, {
      editor: this.state.editor,
    });
  }

  render() {
    const { selectedSourceIsBlackBoxed, skipPausing } = this.props;
    return div(
      {
        className: classnames("editor-wrapper", {
          blackboxed: selectedSourceIsBlackBoxed,
          "skip-pausing": skipPausing,
        }),
        ref: c => (this.$editorWrapper = c),
      },
      div({
        className: "editor-mount devtools-monospace",
        style: this.getInlineEditorStyles(),
      }),
      this.renderSearchInFileBar(),
      this.renderItems()
    );
  }
}

Editor.contextTypes = {
  shortcuts: PropTypes.object,
};

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);
  const selectedLocation = getSelectedLocation(state);

  return {
    selectedLocation,
    selectedSource,
    selectedSourceTextContent: getSelectedSourceTextContent(state),
    selectedSourceIsBlackBoxed: selectedSource
      ? isSourceBlackBoxed(state, selectedSource)
      : null,
    isSourceOnIgnoreList:
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, selectedSource),
    searchInFileEnabled: getActiveSearch(state) === "file",
    conditionalPanelLocation: getConditionalPanelLocation(state),
    symbols: getSymbols(state, selectedLocation),
    isPaused: getIsCurrentThreadPaused(state),
    skipPausing: getSkipPausing(state),
    inlinePreviewEnabled: getInlinePreview(state),
    blackboxedRanges: getBlackBoxRanges(state),
    breakableLines: getSelectedBreakableLines(state),
    highlightedLineRange: getHighlightedLineRangeForSelectedSource(state),
    mapScopesEnabled: selectedSource?.isOriginal
      ? isMapScopesEnabled(state)
      : null,
  };
};

const mapDispatchToProps = dispatch => ({
  ...bindActionCreators(
    {
      openConditionalPanel: actions.openConditionalPanel,
      closeConditionalPanel: actions.closeConditionalPanel,
      continueToHere: actions.continueToHere,
      toggleBreakpointAtLine: actions.toggleBreakpointAtLine,
      addBreakpointAtLine: actions.addBreakpointAtLine,
      jumpToMappedLocation: actions.jumpToMappedLocation,
      updateViewport: actions.updateViewport,
      updateCursorPosition: actions.updateCursorPosition,
      closeTab: actions.closeTab,
      showEditorContextMenu: actions.showEditorContextMenu,
      showEditorGutterContextMenu: actions.showEditorGutterContextMenu,
      selectLocation: actions.selectLocation,
    },
    dispatch
  ),
});

export default connect(mapStateToProps, mapDispatchToProps)(Editor);
