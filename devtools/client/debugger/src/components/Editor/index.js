/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React, { PureComponent } from "react";
import { bindActionCreators } from "redux";
import ReactDOM from "react-dom";
import { connect } from "../../utils/connect";

import { getLineText, isLineBlackboxed } from "./../../utils/source";
import { createLocation } from "./../../utils/location";
import { getIndentation } from "../../utils/indentation";

import { showMenu } from "../../context-menu/menu";
import {
  createBreakpointItems,
  breakpointItemActions,
} from "./menus/breakpoints";

import {
  continueToHereItem,
  editorItemActions,
  blackBoxLineMenuItem,
} from "./menus/editor";

import {
  getActiveSearch,
  getSelectedLocation,
  getSelectedSource,
  getSelectedSourceTextContent,
  getSelectedBreakableLines,
  getConditionalPanelLocation,
  getSymbols,
  getIsCurrentThreadPaused,
  getThreadContext,
  getSkipPausing,
  getInlinePreview,
  getEditorWrapping,
  getBlackBoxRanges,
  isSourceBlackBoxed,
  getHighlightedLineRangeForSelectedSource,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../selectors";

// Redux actions
import actions from "../../actions";

import SearchInFileBar from "./SearchInFileBar";
import HighlightLines from "./HighlightLines";
import Preview from "./Preview";
import Breakpoints from "./Breakpoints";
import ColumnBreakpoints from "./ColumnBreakpoints";
import DebugLine from "./DebugLine";
import HighlightLine from "./HighlightLine";
import EmptyLines from "./EmptyLines";
import EditorMenu from "./EditorMenu";
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
  scrollToColumn,
  toEditorPosition,
  getSourceLocationFromMouseEvent,
  hasDocument,
  onMouseOver,
  startOperation,
  endOperation,
} from "../../utils/editor";

import { resizeToggleButton, resizeBreakpointGutter } from "../../utils/ui";

const { debounce } = require("devtools/shared/debounce");
const classnames = require("devtools/client/shared/classnames.js");

const { appinfo } = Services;
const isMacOS = appinfo.OS === "Darwin";

function isSecondary(ev) {
  return isMacOS && ev.ctrlKey && ev.button === 0;
}

function isCmd(ev) {
  return isMacOS ? ev.metaKey : ev.ctrlKey;
}

import "./Editor.css";
import "./Breakpoints.css";
import "./InlinePreview.css";

const cssVars = {
  searchbarHeight: "var(--editor-searchbar-height)",
};

class Editor extends PureComponent {
  static get propTypes() {
    return {
      selectedSource: PropTypes.object,
      selectedSourceTextContent: PropTypes.object,
      selectedSourceIsBlackBoxed: PropTypes.bool,
      cx: PropTypes.object.isRequired,
      closeTab: PropTypes.func.isRequired,
      toggleBreakpointAtLine: PropTypes.func.isRequired,
      conditionalPanelLocation: PropTypes.object,
      closeConditionalPanel: PropTypes.func.isRequired,
      openConditionalPanel: PropTypes.func.isRequired,
      updateViewport: PropTypes.func.isRequired,
      isPaused: PropTypes.bool.isRequired,
      breakpointActions: PropTypes.object.isRequired,
      editorActions: PropTypes.object.isRequired,
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
      editorWrappingEnabled: PropTypes.bool.isRequired,
      skipPausing: PropTypes.bool.isRequired,
      blackboxedRanges: PropTypes.object.isRequired,
      breakableLines: PropTypes.object.isRequired,
      highlightedLineRange: PropTypes.object,
      isSourceOnIgnoreList: PropTypes.bool,
    };
  }

  $editorWrapper;
  constructor(props) {
    super(props);

    this.state = {
      editor: null,
      contextMenu: null,
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
      nextProps.selectedSourceTextContent !==
        this.props.selectedSourceTextContent ||
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
    const codeMirrorWrapper = codeMirror.getWrapperElement();

    codeMirror.on("gutterClick", this.onGutterClick);

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
    const { cx, selectedSource } = this.props;
    if (selectedSource) {
      e.preventDefault();
      e.stopPropagation();
      this.props.closeTab(cx, selectedSource, "shortcut");
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

    this.props.toggleBreakpointAtLine(this.props.cx, line);
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
      cx,
      selectedSource,
      selectedSourceTextContent,
      breakpointActions,
      editorActions,
      isPaused,
      conditionalPanelLocation,
      closeConditionalPanel,
      isSourceOnIgnoreList,
      blackboxedRanges,
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

    const location = createLocation({
      line,
      column: undefined,
      source: selectedSource,
    });

    if (target.classList.contains("CodeMirror-linenumber")) {
      const lineText = getLineText(
        sourceId,
        selectedSourceTextContent,
        line
      ).trim();

      showMenu(event, [
        ...createBreakpointItems(cx, location, breakpointActions, lineText),
        { type: "separator" },
        continueToHereItem(cx, location, isPaused, editorActions),
        { type: "separator" },
        blackBoxLineMenuItem(
          cx,
          selectedSource,
          editorActions,
          editor,
          blackboxedRanges,
          isSourceOnIgnoreList,
          line
        ),
      ]);
      return;
    }

    if (target.getAttribute("id") === "columnmarker") {
      return;
    }

    this.setState({ contextMenu: event });
  }

  clearContextMenu = () => {
    this.setState({ contextMenu: null });
  };

  onGutterClick = (cm, line, gutter, ev) => {
    const {
      cx,
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
        cx,
        createLocation({
          line: sourceLine,
          column: undefined,
          source: selectedSource,
        })
      );
      return;
    }

    addBreakpointAtLine(
      cx,
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
    const { cx, selectedSource, updateCursorPosition, jumpToMappedLocation } =
      this.props;

    if (selectedSource) {
      const sourceLocation = getSourceLocationFromMouseEvent(
        this.state.editor,
        selectedSource,
        e
      );

      if (e.metaKey && e.altKey) {
        jumpToMappedLocation(cx, sourceLocation);
      }

      updateCursorPosition(sourceLocation);
    }
  }

  shouldScrollToLocation(nextProps, editor) {
    const { selectedLocation, selectedSource, selectedSourceTextContent } =
      this.props;
    if (
      !editor ||
      !nextProps.selectedSource ||
      !nextProps.selectedLocation ||
      !nextProps.selectedLocation.line ||
      !nextProps.selectedSourceTextContent
    ) {
      return false;
    }

    const isFirstLoad =
      (!selectedSource || !selectedSourceTextContent) &&
      nextProps.selectedSourceTextContent;
    const locationChanged = selectedLocation !== nextProps.selectedLocation;
    const symbolsChanged = nextProps.symbols != this.props.symbols;

    return isFirstLoad || locationChanged || symbolsChanged;
  }

  scrollToLocation(nextProps, editor) {
    const { selectedLocation, selectedSource } = nextProps;

    let { line, column } = toEditorPosition(selectedLocation);

    if (selectedSource && hasDocument(selectedSource.id)) {
      const doc = getDocument(selectedSource.id);
      const lineText = doc.getLine(line);
      column = Math.max(column, getIndentation(lineText));
    }

    scrollToColumn(editor.codeMirror, line, column);
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
      cx,
      selectedSource,
      conditionalPanelLocation,
      isPaused,
      inlinePreviewEnabled,
      editorWrappingEnabled,
      highlightedLineRange,
      blackboxedRanges,
      isSourceOnIgnoreList,
      selectedSourceIsBlackBoxed,
    } = this.props;
    const { editor, contextMenu } = this.state;

    if (!selectedSource || !editor || !getDocument(selectedSource.id)) {
      return null;
    }

    return (
      <div>
        <DebugLine />
        <HighlightLine />
        <EmptyLines editor={editor} />
        <Breakpoints editor={editor} cx={cx} />
        <Preview editor={editor} editorRef={this.$editorWrapper} />
        {highlightedLineRange ? (
          <HighlightLines editor={editor} range={highlightedLineRange} />
        ) : null}
        {isSourceOnIgnoreList || selectedSourceIsBlackBoxed ? (
          <BlackboxLines
            editor={editor}
            selectedSource={selectedSource}
            isSourceOnIgnoreList={isSourceOnIgnoreList}
            blackboxedRangesForSelectedSource={
              blackboxedRanges[selectedSource.url]
            }
          />
        ) : null}
        <Exceptions />
        <EditorMenu
          editor={editor}
          contextMenu={contextMenu}
          clearContextMenu={this.clearContextMenu}
          selectedSource={selectedSource}
          editorWrappingEnabled={editorWrappingEnabled}
        />
        {conditionalPanelLocation ? <ConditionalPanel editor={editor} /> : null}
        <ColumnBreakpoints editor={editor} />
        {isPaused && inlinePreviewEnabled ? (
          <InlinePreviews editor={editor} selectedSource={selectedSource} />
        ) : null}
      </div>
    );
  }

  renderSearchInFileBar() {
    if (!this.props.selectedSource) {
      return null;
    }

    return <SearchInFileBar editor={this.state.editor} />;
  }

  render() {
    const { selectedSourceIsBlackBoxed, skipPausing } = this.props;
    return (
      <div
        className={classnames("editor-wrapper", {
          blackboxed: selectedSourceIsBlackBoxed,
          "skip-pausing": skipPausing,
        })}
        ref={c => (this.$editorWrapper = c)}
      >
        <div
          className="editor-mount devtools-monospace"
          style={this.getInlineEditorStyles()}
        />
        {this.renderSearchInFileBar()}
        {this.renderItems()}
      </div>
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
    cx: getThreadContext(state),
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
    editorWrappingEnabled: getEditorWrapping(state),
    blackboxedRanges: getBlackBoxRanges(state),
    breakableLines: getSelectedBreakableLines(state),
    highlightedLineRange: getHighlightedLineRangeForSelectedSource(state),
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
    },
    dispatch
  ),
  breakpointActions: breakpointItemActions(dispatch),
  editorActions: editorItemActions(dispatch),
});

export default connect(mapStateToProps, mapDispatchToProps)(Editor);
