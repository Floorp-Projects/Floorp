/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import PropTypes from "prop-types";
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import { connect } from "react-redux";
import classnames from "classnames";
import { debounce } from "lodash";

import { isLoaded } from "../../utils/source";
import { isFirefox } from "devtools-environment";
import { features } from "../../utils/prefs";
import { getIndentation } from "../../utils/indentation";

import {
  getActiveSearch,
  getSelectedLocation,
  getSelectedSource,
  getConditionalPanelLocation,
  getSymbols
} from "../../selectors";

// Redux actions
import actions from "../../actions";

import Footer from "./Footer";
import SearchBar from "./SearchBar";
import HighlightLines from "./HighlightLines";
import Preview from "./Preview";
import Breakpoints from "./Breakpoints";
import ColumnBreakpoints from "./ColumnBreakpoints";
import DebugLine from "./DebugLine";
import HighlightLine from "./HighlightLine";
import EmptyLines from "./EmptyLines";
import GutterMenu from "./GutterMenu";
import EditorMenu from "./EditorMenu";
import ConditionalPanel from "./ConditionalPanel";

import {
  showSourceText,
  updateDocument,
  showLoading,
  showErrorMessage,
  shouldShowFooter,
  getEditor,
  clearEditor,
  getCursorLine,
  toSourceLine,
  getDocument,
  scrollToColumn,
  toEditorPosition,
  getSourceLocationFromMouseEvent,
  hasDocument,
  onMouseOver,
  startOperation,
  endOperation
} from "../../utils/editor";

import { resizeToggleButton, resizeBreakpointGutter } from "../../utils/ui";

import "./Editor.css";
import "./Highlight.css";

import type SourceEditor from "../../utils/editor/source-editor";
import type { SymbolDeclarations } from "../../workers/parser";
import type { SourceLocation, Source } from "../../types";

const cssVars = {
  searchbarHeight: "var(--editor-searchbar-height)",
  footerHeight: "var(--editor-footer-height)"
};

export type Props = {
  selectedLocation: ?SourceLocation,
  selectedSource: ?Source,
  searchOn: boolean,
  horizontal: boolean,
  startPanelSize: number,
  endPanelSize: number,
  conditionalPanelLocation: SourceLocation,
  symbols: SymbolDeclarations,

  // Actions
  openConditionalPanel: (?SourceLocation) => void,
  closeConditionalPanel: void => void,
  setContextMenu: (string, any) => void,
  continueToHere: number => void,
  toggleBreakpoint: number => void,
  toggleBreakpointsAtLine: number => void,
  addOrToggleDisabledBreakpoint: number => void,
  jumpToMappedLocation: any => void,
  traverseResults: (boolean, Object) => void,
  updateViewport: void => void
};

type State = {
  editor: SourceEditor
};

class Editor extends PureComponent<Props, State> {
  $editorWrapper: ?HTMLDivElement;
  constructor(props: Props) {
    super(props);

    this.state = {
      highlightedLineRange: null,
      editor: null
    };
  }

  componentWillReceiveProps(nextProps) {
    if (!this.state.editor) {
      return;
    }

    startOperation();
    resizeBreakpointGutter(this.state.editor.codeMirror);
    resizeToggleButton(this.state.editor.codeMirror);
    endOperation();
  }

  componentWillUpdate(nextProps) {
    if (!this.state.editor) {
      return;
    }

    this.setText(nextProps);
    this.setSize(nextProps);
    this.scrollToLocation(nextProps);
  }

  setupEditor() {
    const editor = getEditor();

    // disables the default search shortcuts
    // $FlowIgnore
    editor._initShortcuts = () => {};

    const node = ReactDOM.findDOMNode(this);
    if (node instanceof HTMLElement) {
      editor.appendToLocalElement(node.querySelector(".editor-mount"));
    }

    const { codeMirror } = editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();

    startOperation();
    resizeBreakpointGutter(codeMirror);
    resizeToggleButton(codeMirror);
    endOperation();

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

    if (!isFirefox()) {
      codeMirror.on("gutterContextMenu", (cm, line, eventName, event) =>
        this.onGutterContextMenu(event)
      );
      codeMirror.on("contextmenu", (cm, event) => this.openMenu(event));
    } else {
      codeMirrorWrapper.addEventListener("contextmenu", event =>
        this.openMenu(event)
      );
    }

    codeMirror.on("scroll", this.onEditorScroll);
    this.onEditorScroll();

    this.setState({ editor });
    return editor;
  }

  componentDidMount() {
    const { shortcuts } = this.context;

    const searchAgainKey = L10N.getStr("sourceSearch.search.again.key2");
    const searchAgainPrevKey = L10N.getStr(
      "sourceSearch.search.againPrev.key2"
    );

    shortcuts.on(L10N.getStr("toggleBreakpoint.key"), this.onToggleBreakpoint);
    shortcuts.on(
      L10N.getStr("toggleCondPanel.key"),
      this.onToggleConditionalPanel
    );
    shortcuts.on("Esc", this.onEscape);
    shortcuts.on(searchAgainPrevKey, this.onSearchAgain);
    shortcuts.on(searchAgainKey, this.onSearchAgain);
  }

  componentWillUnmount() {
    if (this.state.editor) {
      this.state.editor.destroy();
      this.state.editor.codeMirror.off("scroll", this.onEditorScroll);
      this.setState({ editor: null });
    }

    const searchAgainKey = L10N.getStr("sourceSearch.search.again.key2");
    const searchAgainPrevKey = L10N.getStr(
      "sourceSearch.search.againPrev.key2"
    );
    const shortcuts = this.context.shortcuts;
    shortcuts.off(L10N.getStr("toggleBreakpoint.key"));
    shortcuts.off(L10N.getStr("toggleCondPanel.key"));
    shortcuts.off(searchAgainPrevKey);
    shortcuts.off(searchAgainKey);
  }

  componentDidUpdate(prevProps, prevState) {
    const { selectedSource } = this.props;
    // NOTE: when devtools are opened, the editor is not set when
    // the source loads so we need to wait until the editor is
    // set to update the text and size.
    if (!prevState.editor && selectedSource) {
      if (!this.state.editor) {
        const editor = this.setupEditor();
        updateDocument(editor, selectedSource);
      } else {
        this.setText(this.props);
        this.setSize(this.props);
      }
    }
  }

  getCurrentLine() {
    const { codeMirror } = this.state.editor;
    const { selectedSource } = this.props;
    if (!selectedSource) {
      return;
    }

    const line = getCursorLine(codeMirror);
    return toSourceLine(selectedSource.id, line);
  }

  onToggleBreakpoint = (key, e: KeyboardEvent) => {
    e.preventDefault();
    e.stopPropagation();
    const { selectedSource, conditionalPanelLocation } = this.props;

    if (!selectedSource) {
      return;
    }

    const line = this.getCurrentLine();
    if (typeof line !== "number") {
      return;
    }

    if (e.shiftKey) {
      this.toggleConditionalPanel(line);
    } else if (!conditionalPanelLocation) {
      this.props.toggleBreakpoint(line);
    } else {
      this.toggleConditionalPanel(line);
      this.props.toggleBreakpoint(line);
    }
  };

  onToggleConditionalPanel = (key, e: KeyboardEvent) => {
    e.stopPropagation();
    e.preventDefault();
    const line = this.getCurrentLine();
    if (typeof line !== "number") {
      return;
    }

    this.toggleConditionalPanel(line);
  };

  onEditorScroll = debounce(this.props.updateViewport, 200);

  onKeyDown(e: KeyboardEvent) {
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
  onEscape = (key, e: KeyboardEvent) => {
    if (!this.state.editor) {
      return;
    }

    const { codeMirror } = this.state.editor;
    if (codeMirror.listSelections().length > 1) {
      codeMirror.execCommand("singleSelection");
      e.preventDefault();
    }
  };

  onSearchAgain = (_, e: KeyboardEvent) => {
    this.props.traverseResults(e.shiftKey, this.state.editor);
  };

  openMenu(event: MouseEvent) {
    event.stopPropagation();
    event.preventDefault();

    const { setContextMenu } = this.props;
    const target: Element = (event.target: any);
    if (target.classList.contains("CodeMirror-linenumber")) {
      return setContextMenu("Gutter", event);
    }

    return setContextMenu("Editor", event);
  }

  onGutterClick = (
    cm: Object,
    line: number,
    gutter: string,
    ev: MouseEvent
  ) => {
    const {
      selectedSource,
      conditionalPanelLocation,
      closeConditionalPanel,
      addOrToggleDisabledBreakpoint,
      toggleBreakpointsAtLine,
      continueToHere
    } = this.props;

    // ignore right clicks in the gutter
    if (
      (ev.ctrlKey && ev.button === 0) ||
      ev.button === 2 ||
      (selectedSource && selectedSource.isBlackBoxed) ||
      !selectedSource
    ) {
      return;
    }

    if (conditionalPanelLocation) {
      return closeConditionalPanel();
    }

    if (gutter === "CodeMirror-foldgutter") {
      return;
    }

    const sourceLine = toSourceLine(selectedSource.id, line);
    if (typeof sourceLine !== "number") {
      return;
    }

    if (ev.metaKey) {
      return continueToHere(sourceLine);
    }

    if (ev.shiftKey) {
      return addOrToggleDisabledBreakpoint(sourceLine);
    }

    return toggleBreakpointsAtLine(sourceLine);
  };

  onGutterContextMenu = (event: MouseEvent) => {
    event.stopPropagation();
    event.preventDefault();
    return this.props.setContextMenu("Gutter", event);
  };

  onClick(e: MouseEvent) {
    const { selectedLocation, jumpToMappedLocation } = this.props;

    if (selectedLocation && e.metaKey && e.altKey) {
      const sourceLocation = getSourceLocationFromMouseEvent(
        this.state.editor,
        selectedLocation,
        e
      );
      jumpToMappedLocation(sourceLocation);
    }
  }

  toggleConditionalPanel = line => {
    const {
      conditionalPanelLocation,
      closeConditionalPanel,
      openConditionalPanel
    } = this.props;

    if (conditionalPanelLocation) {
      return closeConditionalPanel();
    }

    return openConditionalPanel(conditionalPanelLocation);
  };

  closeConditionalPanel = () => {
    return this.props.closeConditionalPanel();
  };

  shouldScrollToLocation(nextProps) {
    const { selectedLocation, selectedSource } = this.props;
    const { editor } = this.state;

    if (
      !editor ||
      !nextProps.selectedSource ||
      !nextProps.selectedLocation ||
      !nextProps.selectedLocation.line ||
      !isLoaded(nextProps.selectedSource)
    ) {
      return false;
    }

    const isFirstLoad =
      (!selectedSource || !isLoaded(selectedSource)) &&
      isLoaded(nextProps.selectedSource);
    const locationChanged = selectedLocation !== nextProps.selectedLocation;
    const symbolsChanged = nextProps.symbols != this.props.symbols;

    return isFirstLoad || locationChanged || symbolsChanged;
  }

  scrollToLocation(nextProps) {
    const { editor } = this.state;
    const { selectedLocation, selectedSource } = nextProps;

    if (selectedLocation && this.shouldScrollToLocation(nextProps)) {
      let { line, column } = toEditorPosition(selectedLocation);

      if (selectedSource && hasDocument(selectedSource.id)) {
        const doc = getDocument(selectedSource.id);
        const lineText: ?string = doc.getLine(line);
        column = Math.max(column, getIndentation(lineText));
      }
      scrollToColumn(editor.codeMirror, line, column);
    }
  }

  setSize(nextProps) {
    if (!this.state.editor) {
      return;
    }

    if (
      nextProps.startPanelSize !== this.props.startPanelSize ||
      nextProps.endPanelSize !== this.props.endPanelSize
    ) {
      this.state.editor.codeMirror.setSize();
    }
  }

  setText(props) {
    const { selectedSource, symbols } = props;

    if (!this.state.editor) {
      return;
    }

    // check if we previously had a selected source
    if (!selectedSource) {
      return this.clearEditor();
    }

    if (!isLoaded(selectedSource)) {
      return showLoading(this.state.editor);
    }

    if (selectedSource.error) {
      return this.showErrorMessage(selectedSource.error);
    }

    if (selectedSource) {
      return showSourceText(this.state.editor, selectedSource, symbols);
    }
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
    const { selectedSource, horizontal, searchOn } = this.props;

    const subtractions = [];

    if (shouldShowFooter(selectedSource, horizontal)) {
      subtractions.push(cssVars.footerHeight);
    }

    if (searchOn) {
      subtractions.push(cssVars.searchbarHeight);
    }

    return {
      height:
        subtractions.length === 0
          ? "100%"
          : `calc(100% - ${subtractions.join(" - ")})`
    };
  }

  renderItems() {
    const { horizontal, selectedSource, conditionalPanelLocation } = this.props;
    const { editor } = this.state;

    if (!editor || !selectedSource) {
      return null;
    }

    return (
      <div>
        <DebugLine editor={editor} />
        <HighlightLine />
        <EmptyLines editor={editor} />
        <Breakpoints editor={editor} />
        <Preview editor={editor} editorRef={this.$editorWrapper} />;
        <Footer editor={editor} horizontal={horizontal} />
        <HighlightLines editor={editor} />
        <EditorMenu editor={editor} />
        <GutterMenu editor={editor} />
        {conditionalPanelLocation ? <ConditionalPanel editor={editor} /> : null}
        {features.columnBreakpoints ? (
          <ColumnBreakpoints editor={editor} />
        ) : null}
      </div>
    );
  }

  renderSearchBar() {
    const { editor } = this.state;

    if (!editor) {
      return null;
    }

    return <SearchBar editor={editor} />;
  }

  render() {
    return (
      <div
        className={classnames("editor-wrapper")}
        ref={c => (this.$editorWrapper = c)}
      >
        <div
          className="editor-mount devtools-monospace"
          style={this.getInlineEditorStyles()}
        />
        {this.renderSearchBar()}
        {this.renderItems()}
      </div>
    );
  }
}

Editor.contextTypes = {
  shortcuts: PropTypes.object
};

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);

  return {
    selectedLocation: getSelectedLocation(state),
    selectedSource,
    searchOn: getActiveSearch(state) === "file",
    conditionalPanelLocation: getConditionalPanelLocation(state),
    symbols: getSymbols(state, selectedSource)
  };
};

export default connect(
  mapStateToProps,
  {
    openConditionalPanel: actions.openConditionalPanel,
    closeConditionalPanel: actions.closeConditionalPanel,
    setContextMenu: actions.setContextMenu,
    continueToHere: actions.continueToHere,
    toggleBreakpoint: actions.toggleBreakpoint,
    toggleBreakpointsAtLine: actions.toggleBreakpointsAtLine,
    addOrToggleDisabledBreakpoint: actions.addOrToggleDisabledBreakpoint,
    jumpToMappedLocation: actions.jumpToMappedLocation,
    traverseResults: actions.traverseResults,
    updateViewport: actions.updateViewport
  }
)(Editor);
