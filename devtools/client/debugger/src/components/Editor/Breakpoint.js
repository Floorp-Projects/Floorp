/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PureComponent } from "react";
import PropTypes from "prop-types";

import { getDocument, toEditorLine } from "../../utils/editor";
import { getSelectedLocation } from "../../utils/selected-location";
import { features } from "../../utils/prefs";
const classnames = require("devtools/client/shared/classnames.js");

const breakpointSvg = document.createElement("div");
breakpointSvg.innerHTML =
  '<svg viewBox="0 0 60 15" width="60" height="15"><path d="M53.07.5H1.5c-.54 0-1 .46-1 1v12c0 .54.46 1 1 1h51.57c.58 0 1.15-.26 1.53-.7l4.7-6.3-4.7-6.3c-.38-.44-.95-.7-1.53-.7z"/></svg>';

class Breakpoint extends PureComponent {
  static get propTypes() {
    return {
      breakpoint: PropTypes.object.isRequired,
      editor: PropTypes.object.isRequired,
      selectedSource: PropTypes.object,
    };
  }

  componentDidMount() {
    this.addBreakpoint(this.props);
  }

  componentDidUpdate(prevProps) {
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
    bp.oncontextmenu = this.onContextMenu;

    return bp;
  }

  onClick = event => {
    const {
      continueToHere,
      toggleBreakpointsAtLine,
      removeBreakpointsAtLine,
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
      continueToHere(selectedLocation);
      return;
    }

    if (event.shiftKey) {
      toggleBreakpointsAtLine(!breakpoint.disabled, selectedLocation.line);
      return;
    }

    removeBreakpointsAtLine(selectedLocation.source, selectedLocation.line);
  };

  onContextMenu = event => {
    event.stopPropagation();
    event.preventDefault();

    this.props.showEditorEditBreakpointContextMenu(
      event,
      this.props.breakpoint
    );
  };

  addBreakpoint(props) {
    const { breakpoint, editor, selectedSource } = props;
    // Hidden Breakpoints are never rendered on the client
    if (breakpoint.options.hidden) {
      return;
    }

    if (!selectedSource) {
      return;
    }

    const doc = getDocument(selectedSource.id);
    if (!doc) {
      return;
    }

    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    const line = toEditorLine(selectedSource.id, selectedLocation.line);

    doc.setGutterMarker(line, "breakpoints", this.makeMarker());

    editor.codeMirror.addLineClass(line, "wrap", "new-breakpoint");
    editor.codeMirror.removeLineClass(line, "wrap", "breakpoint-disabled");
    editor.codeMirror.removeLineClass(line, "wrap", "has-condition");
    editor.codeMirror.removeLineClass(line, "wrap", "has-log");

    if (breakpoint.disabled) {
      editor.codeMirror.addLineClass(line, "wrap", "breakpoint-disabled");
    }

    if (breakpoint.options.logValue) {
      editor.codeMirror.addLineClass(line, "wrap", "has-log");
    } else if (breakpoint.options.condition) {
      editor.codeMirror.addLineClass(line, "wrap", "has-condition");
    }
  }

  removeBreakpoint(props) {
    const { selectedSource, breakpoint } = props;
    if (!selectedSource) {
      return;
    }

    const doc = getDocument(selectedSource.id);
    if (!doc) {
      return;
    }

    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    const line = toEditorLine(selectedSource.id, selectedLocation.line);

    doc.setGutterMarker(line, "breakpoints", null);
    doc.removeLineClass(line, "wrap", "new-breakpoint");
    doc.removeLineClass(line, "wrap", "breakpoint-disabled");
    doc.removeLineClass(line, "wrap", "has-condition");
    doc.removeLineClass(line, "wrap", "has-log");
  }

  render() {
    return null;
  }
}

export default Breakpoint;
