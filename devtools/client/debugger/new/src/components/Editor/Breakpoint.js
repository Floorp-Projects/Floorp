/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import classnames from "classnames";
import Svg from "../shared/Svg";

import { getDocument, toEditorLine } from "../../utils/editor";
import { features } from "../../utils/prefs";

import type { Source, Breakpoint as BreakpointType } from "../../types";

const breakpointSvg = document.createElement("div");
ReactDOM.render(<Svg name="breakpoint" />, breakpointSvg);

function makeMarker(isDisabled: boolean) {
  const bp = breakpointSvg.cloneNode(true);
  bp.className = classnames("editor new-breakpoint", {
    "breakpoint-disabled": isDisabled,
    "folding-enabled": features.codeFolding
  });

  return bp;
}

type Props = {
  breakpoint: BreakpointType,
  selectedSource: Source,
  editor: Object
};

class Breakpoint extends PureComponent<Props> {
  addBreakpoint: Function;

  addBreakpoint = () => {
    const { breakpoint, editor, selectedSource } = this.props;

    // Hidden Breakpoints are never rendered on the client
    if (breakpoint.hidden) {
      return;
    }

    // NOTE: we need to wait for the breakpoint to be loaded
    // to get the generated location
    if (!selectedSource || breakpoint.loading) {
      return;
    }

    const sourceId = selectedSource.id;
    const line = toEditorLine(sourceId, breakpoint.location.line);

    editor.codeMirror.setGutterMarker(
      line,
      "breakpoints",
      makeMarker(breakpoint.disabled)
    );

    editor.codeMirror.addLineClass(line, "line", "new-breakpoint");
    if (breakpoint.condition) {
      if (breakpoint.log) {
        editor.codeMirror.addLineClass(line, "line", "has-condition log");
      } else {
        editor.codeMirror.addLineClass(line, "line", "has-condition");
      }
    } else {
      editor.codeMirror.removeLineClass(line, "line", "has-condition");
    }
  };

  componentDidMount() {
    this.addBreakpoint();
  }

  componentDidUpdate() {
    this.addBreakpoint();
  }

  componentWillUnmount() {
    const { editor, breakpoint, selectedSource } = this.props;

    if (!selectedSource) {
      return;
    }

    if (breakpoint.loading) {
      return;
    }

    const sourceId = selectedSource.id;
    const doc = getDocument(sourceId);
    if (!doc) {
      return;
    }

    const line = toEditorLine(sourceId, breakpoint.location.line);

    // NOTE: when we upgrade codemirror we can use `doc.setGutterMarker`
    if (doc.setGutterMarker) {
      doc.setGutterMarker(line, "breakpoints", null);
    } else {
      editor.codeMirror.setGutterMarker(line, "breakpoints", null);
    }

    doc.removeLineClass(line, "line", "new-breakpoint");
    doc.removeLineClass(line, "line", "has-condition");
  }

  render() {
    return null;
  }
}

export default Breakpoint;
