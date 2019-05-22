/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { PureComponent } from "react";
import classnames from "classnames";

import { getDocument, toEditorLine } from "../../utils/editor";
import { getSelectedLocation } from "../../utils/selected-location";
import { features } from "../../utils/prefs";
import { showMenu } from "devtools-contextmenu";
import { breakpointItems } from "./menus/breakpoints";
import type { BreakpointItemActions } from "./menus/breakpoints";
import type { EditorItemActions } from "./menus/editor";

import type {
  Source,
  Breakpoint as BreakpointType,
  ThreadContext,
} from "../../types";

const breakpointSvg = document.createElement("div");
breakpointSvg.innerHTML =
  '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 60 15" width="60" height="15"><path d="M53.07.5H1.5c-.54 0-1 .46-1 1v12c0 .54.46 1 1 1h51.57c.58 0 1.15-.26 1.53-.7l4.7-6.3-4.7-6.3c-.38-.44-.95-.7-1.53-.7z"/></svg>';

type Props = {
  cx: ThreadContext,
  breakpoint: BreakpointType,
  selectedSource: Source,
  editor: Object,
  breakpointActions: BreakpointItemActions,
  editorActions: EditorItemActions,
};

class Breakpoint extends PureComponent<Props> {
  componentDidMount() {
    this.addBreakpoint(this.props);
  }

  componentDidUpdate(prevProps: Props) {
    this.removeBreakpoint(prevProps);
    this.addBreakpoint(this.props);
  }

  componentWillUnmount() {
    this.removeBreakpoint(this.props);
  }

  makeMarker() {
    const { breakpoint } = this.props;
    const bp = breakpointSvg.cloneNode(true);

    bp.className = classnames("editor new-breakpoint", {
      "breakpoint-disabled": breakpoint.disabled,
      "folding-enabled": features.codeFolding,
    });

    bp.onmousedown = this.onClick;
    // NOTE: flow does not know about oncontextmenu
    (bp: any).oncontextmenu = this.onContextMenu;

    return bp;
  }

  onClick = (event: MouseEvent) => {
    const {
      cx,
      breakpointActions,
      editorActions,
      breakpoint,
      selectedSource,
    } = this.props;

    // ignore right clicks
    if ((event.ctrlKey && event.button === 0) || event.button === 2) {
      return;
    }

    event.stopPropagation();
    event.preventDefault();

    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    if (event.metaKey) {
      return editorActions.continueToHere(cx, selectedLocation.line);
    }

    if (event.shiftKey) {
      if (features.columnBreakpoints) {
        return breakpointActions.toggleBreakpointsAtLine(
          cx,
          !breakpoint.disabled,
          selectedLocation.line
        );
      }

      return breakpointActions.toggleDisabledBreakpoint(cx, breakpoint);
    }

    return breakpointActions.removeBreakpointsAtLine(
      cx,
      selectedLocation.sourceId,
      selectedLocation.line
    );
  };

  onContextMenu = (event: MouseEvent) => {
    const { cx, breakpoint, selectedSource, breakpointActions } = this.props;
    event.stopPropagation();
    event.preventDefault();
    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);

    showMenu(
      event,
      breakpointItems(cx, breakpoint, selectedLocation, breakpointActions)
    );
  };

  addBreakpoint(props: Props) {
    const { breakpoint, editor, selectedSource } = props;
    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);

    // Hidden Breakpoints are never rendered on the client
    if (breakpoint.options.hidden) {
      return;
    }

    if (!selectedSource) {
      return;
    }

    const sourceId = selectedSource.id;
    const line = toEditorLine(sourceId, selectedLocation.line);
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
  }

  removeBreakpoint(props: Props) {
    const { selectedSource, breakpoint } = props;
    if (!selectedSource) {
      return;
    }

    const sourceId = selectedSource.id;
    const doc = getDocument(sourceId);

    if (!doc) {
      return;
    }

    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    const line = toEditorLine(sourceId, selectedLocation.line);

    doc.setGutterMarker(line, "breakpoints", null);
    doc.removeLineClass(line, "line", "new-breakpoint");
    doc.removeLineClass(line, "line", "has-condition");
    doc.removeLineClass(line, "line", "has-log");
  }

  render() {
    return null;
  }
}

export default Breakpoint;
