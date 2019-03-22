/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import classnames from "classnames";
import Svg from "../shared/Svg";

import { getDocument, toEditorLine } from "../../utils/editor";
import { getSelectedLocation } from "../../utils/source-maps";
import { features } from "../../utils/prefs";
import { showMenu } from "devtools-contextmenu";
import { breakpointItems } from "./menus/breakpoints";
import type { BreakpointItemActions } from "./menus/breakpoints";
import type { EditorItemActions } from "./menus/editor";

import type { Source, Breakpoint as BreakpointType } from "../../types";

const breakpointSvg = document.createElement("div");
ReactDOM.render(<Svg name="breakpoint" />, breakpointSvg);

type Props = {
  breakpoint: BreakpointType,
  selectedSource: Source,
  editor: Object,
  breakpointActions: BreakpointItemActions,
  editorActions: EditorItemActions
};

class Breakpoint extends PureComponent<Props> {
  componentDidMount() {
    this.addBreakpoint();
  }

  componentDidUpdate() {
    this.addBreakpoint();
  }

  componentWillUnmount() {
    const { breakpoint, selectedSource } = this.props;
    if (!selectedSource || breakpoint.loading) {
      return;
    }

    const sourceId = selectedSource.id;
    const doc = getDocument(sourceId);

    if (!doc) {
      return;
    }

    const line = toEditorLine(sourceId, this.selectedLocation.line);

    doc.setGutterMarker(line, "breakpoints", null);
    doc.removeLineClass(line, "line", "new-breakpoint");
    doc.removeLineClass(line, "line", "has-condition");
    doc.removeLineClass(line, "line", "has-log");
  }

  get selectedLocation() {
    const { breakpoint, selectedSource } = this.props;
    return getSelectedLocation(breakpoint, selectedSource);
  }

  makeMarker() {
    const { breakpoint } = this.props;
    const bp = breakpointSvg.cloneNode(true);

    bp.className = classnames("editor new-breakpoint", {
      "breakpoint-disabled": breakpoint.disabled,
      "folding-enabled": features.codeFolding
    });

    bp.onmousedown = this.onClick;
    // NOTE: flow does not know about oncontextmenu
    (bp: any).oncontextmenu = this.onContextMenu;

    return bp;
  }

  onClick = (event: MouseEvent) => {
    const { breakpointActions, editorActions, breakpoint } = this.props;

    // ignore right clicks
    if ((event.ctrlKey && event.button === 0) || event.button === 2) {
      return;
    }

    event.stopPropagation();
    event.preventDefault();

    if (event.metaKey) {
      return editorActions.continueToHere(this.selectedLocation.line);
    }

    if (event.shiftKey) {
      return breakpointActions.toggleDisabledBreakpoint(breakpoint);
    }

    return breakpointActions.removeBreakpointsAtLine(
      this.selectedLocation.sourceId,
      this.selectedLocation.line
    );
  };

  onContextMenu = (event: MouseEvent) => {
    const { breakpoint, breakpointActions } = this.props;
    event.stopPropagation();
    event.preventDefault();
    showMenu(event, breakpointItems(breakpoint, breakpointActions));
  };

  addBreakpoint = () => {
    const { breakpoint, editor, selectedSource } = this.props;

    // Hidden Breakpoints are never rendered on the client
    if (breakpoint.options.hidden) {
      return;
    }

    // NOTE: we need to wait for the breakpoint to be loaded
    // to get the generated location
    if (!selectedSource || breakpoint.loading) {
      return;
    }

    const sourceId = selectedSource.id;
    const line = toEditorLine(sourceId, this.selectedLocation.line);
    const doc = getDocument(sourceId);

    doc.setGutterMarker(line, "breakpoints", this.makeMarker());

    editor.codeMirror.addLineClass(line, "line", "new-breakpoint");
    editor.codeMirror.removeLineClass(line, "line", "has-condition");
    editor.codeMirror.removeLineClass(line, "line", "has-log");

    if (breakpoint.options.logValue) {
      editor.codeMirror.addLineClass(line, "line", "has-log");
    } else if (breakpoint.options.condition) {
      editor.codeMirror.addLineClass(line, "line", "has-condition");
    }
  };

  render() {
    return null;
  }
}

export default Breakpoint;
