/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * CodeMirror source editor utils
 * @module utils/source-editor
 */

const CodeMirror = require("codemirror");

// $FlowIgnore
require("codemirror/lib/codemirror.css");
require("codemirror/mode/javascript/javascript");
require("codemirror/mode/htmlmixed/htmlmixed");
require("codemirror/mode/coffeescript/coffeescript");
require("codemirror/mode/jsx/jsx");
require("codemirror/mode/elm/elm");
require("codemirror/mode/clojure/clojure");
require("codemirror/mode/haxe/haxe");
require("codemirror/addon/search/searchcursor");
require("codemirror/addon/fold/foldcode");
require("codemirror/addon/fold/brace-fold");
require("codemirror/addon/fold/indent-fold");
require("codemirror/addon/fold/foldgutter");
require("codemirror/addon/runmode/runmode");
require("codemirror/addon/selection/active-line");
require("codemirror/addon/edit/matchbrackets");
require("codemirror/addon/display/placeholder");
require("codemirror/mode/clike/clike");
require("codemirror/mode/rust/rust");

require("./source-editor.css");

// NOTE: we should eventually use debugger-html context type mode
type Mode = string | Object;
export type AlignOpts = "top" | "center" | "bottom";

// Maximum allowed margin (in number of lines) from top or bottom of the editor
// while shifting to a line which was initially out of view.
const MAX_VERTICAL_OFFSET = 3;

type SourceEditorOpts = {
  enableCodeFolding: boolean,
  extraKeys: Object,
  gutters: string[],
  foldGutter: boolean,
  lineNumbers: boolean,
  lineWrapping: boolean,
  matchBrackets: boolean,
  mode: string,
  readOnly: boolean,
  showAnnotationRuler: boolean,
  theme: string,
  value: string,
};

export default class SourceEditor {
  opts: SourceEditorOpts;
  editor: any;

  constructor(opts: SourceEditorOpts) {
    this.opts = opts;
  }

  appendToLocalElement(node: any) {
    this.editor = CodeMirror(node, this.opts);
  }

  destroy() {
    // Unlink the current document.
    if (this.editor.doc) {
      this.editor.doc.cm = null;
    }
  }

  get codeMirror(): any {
    return this.editor;
  }

  get CodeMirror() {
    return CodeMirror;
  }

  setText(str: string) {
    this.editor.setValue(str);
  }

  getText() {
    return this.editor.getValue();
  }

  setMode(value: Mode) {
    this.editor.setOption("mode", value);
  }

  /**
   * Replaces the current document with a new source document
   * @memberof utils/source-editor
   */
  replaceDocument(doc: any) {
    this.editor.swapDoc(doc);
  }

  /**
   * Creates a CodeMirror Document
   * @returns CodeMirror.Doc
   * @memberof utils/source-editor
   */
  createDocument() {
    return new CodeMirror.Doc("");
  }

  /**
   * Aligns the provided line to either "top", "center" or "bottom" of the
   * editor view with a maximum margin of MAX_VERTICAL_OFFSET lines from top or
   * bottom.
   * @memberof utils/source-editor
   */
  alignLine(line: number, align: AlignOpts = "top") {
    const cm = this.editor;
    const editorClientRect = cm.getWrapperElement().getBoundingClientRect();

    const from = cm.lineAtHeight(editorClientRect.top, "page");
    const to = cm.lineAtHeight(
      editorClientRect.height + editorClientRect.top,
      "page"
    );

    const linesVisible = to - from;
    const halfVisible = Math.round(linesVisible / 2);

    // If the target line is in view, skip the vertical alignment part.
    if (line <= to && line >= from) {
      return;
    }

    // Setting the offset so that the line always falls in the upper half
    // of visible lines (lower half for bottom aligned).
    // MAX_VERTICAL_OFFSET is the maximum allowed value.
    const offset = Math.min(halfVisible, MAX_VERTICAL_OFFSET);

    let topLine =
      {
        center: Math.max(line - halfVisible, 0),
        bottom: Math.max(line - linesVisible + offset, 0),
        top: Math.max(line - offset, 0),
      }[align || "top"] || offset;

    // Bringing down the topLine to total lines in the editor if exceeding.
    topLine = Math.min(topLine, cm.lineCount());
    this.setFirstVisibleLine(topLine);
  }

  /**
   * Scrolls the view such that the given line number is the first visible line.
   * @memberof utils/source-editor
   */
  setFirstVisibleLine(line: number) {
    const { top } = this.editor.charCoords({ line: line, ch: 0 }, "local");
    this.editor.scrollTo(0, top);
  }
}
