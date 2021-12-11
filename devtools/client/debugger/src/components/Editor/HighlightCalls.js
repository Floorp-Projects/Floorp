/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { connect } from "../../utils/connect";
import {
  getHighlightedCalls,
  getThreadContext,
  getCurrentThread,
} from "../../selectors";
import { getSourceLocationFromMouseEvent } from "../../utils/editor";
import actions from "../../actions";
import "./HighlightCalls.css";

export class HighlightCalls extends Component {
  previousCalls = null;

  componentDidUpdate() {
    this.unhighlightFunctionCalls();
    this.highlightFunctioCalls();
  }

  markCall = call => {
    const { editor } = this.props;
    const startLine = call.location.start.line - 1;
    const endLine = call.location.end.line - 1;
    const startColumn = call.location.start.column;
    const endColumn = call.location.end.column;
    const markedCall = editor.codeMirror.markText(
      { line: startLine, ch: startColumn },
      { line: endLine, ch: endColumn },
      { className: "highlight-function-calls" }
    );
    return markedCall;
  };

  onClick = e => {
    const { editor, selectedSource, cx, continueToHere } = this.props;

    if (selectedSource) {
      const location = getSourceLocationFromMouseEvent(
        editor,
        selectedSource,
        e
      );
      continueToHere(cx, location);
      editor.codeMirror.execCommand("singleSelection");
      editor.codeMirror.execCommand("goGroupLeft");
    }
  };

  highlightFunctioCalls() {
    const { highlightedCalls } = this.props;

    if (!highlightedCalls) {
      return;
    }

    let markedCalls = [];
    markedCalls = highlightedCalls.map(this.markCall);

    const allMarkedElements = document.getElementsByClassName(
      "highlight-function-calls"
    );

    for (let i = 0; i < allMarkedElements.length; i++) {
      allMarkedElements[i].addEventListener("click", this.onClick);
    }

    this.previousCalls = markedCalls;
  }

  unhighlightFunctionCalls() {
    if (!this.previousCalls) {
      return;
    }
    this.previousCalls.forEach(call => call.clear());
    this.previousCalls = null;
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  return {
    highlightedCalls: getHighlightedCalls(state, thread),
    cx: getThreadContext(state),
  };
};

const { continueToHere } = actions;

const mapDispatchToProps = { continueToHere };

export default connect(mapStateToProps, mapDispatchToProps)(HighlightCalls);
