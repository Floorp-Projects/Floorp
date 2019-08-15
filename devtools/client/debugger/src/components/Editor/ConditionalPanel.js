/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import { connect } from "../../utils/connect";
import classNames from "classnames";
import "./ConditionalPanel.css";
import { toEditorLine } from "../../utils/editor";
import actions from "../../actions";

import {
  getBreakpoint,
  getConditionalPanelLocation,
  getLogPointStatus,
  getContext,
} from "../../selectors";

import type { SourceLocation, Context } from "../../types";

function addNewLine(doc: Object) {
  const cursor = doc.getCursor();
  const pos = { line: cursor.line, ch: cursor.ch };
  doc.replaceRange("\n", pos);
}

type Props = {
  cx: Context,
  breakpoint: ?Object,
  setBreakpointOptions: typeof actions.setBreakpointOptions,
  location: SourceLocation,
  log: boolean,
  editor: Object,
  openConditionalPanel: typeof actions.openConditionalPanel,
  closeConditionalPanel: typeof actions.closeConditionalPanel,
};

export class ConditionalPanel extends PureComponent<Props> {
  cbPanel: null | Object;
  input: ?HTMLTextAreaElement;
  codeMirror: ?Object;
  panelNode: ?HTMLDivElement;
  scrollParent: ?HTMLElement;

  constructor() {
    super();
    this.cbPanel = null;
  }

  keepFocusOnInput() {
    if (this.input) {
      this.input.focus();
    }
  }

  saveAndClose = () => {
    if (this.input) {
      this.setBreakpoint(this.input.value.trim());
    }

    this.props.closeConditionalPanel();
  };

  onKey = (e: SyntheticKeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === "Enter") {
      if (this.codeMirror && e.altKey) {
        addNewLine(this.codeMirror.doc);
      } else {
        this.saveAndClose();
      }
    } else if (e.key === "Escape") {
      this.props.closeConditionalPanel();
    }
  };

  setBreakpoint(value: string) {
    const { cx, location, log, breakpoint } = this.props;
    const options = breakpoint ? breakpoint.options : {};
    const type = log ? "logValue" : "condition";
    return this.props.setBreakpointOptions(cx, location, {
      ...options,
      [type]: value,
    });
  }

  clearConditionalPanel() {
    if (this.cbPanel) {
      this.cbPanel.clear();
      this.cbPanel = null;
    }
    if (this.scrollParent) {
      this.scrollParent.removeEventListener("scroll", this.repositionOnScroll);
    }
  }

  repositionOnScroll = () => {
    if (this.panelNode && this.scrollParent) {
      const { scrollLeft } = this.scrollParent;
      this.panelNode.style.transform = `translateX(${scrollLeft}px)`;
    }
  };

  componentWillMount() {
    return this.renderToWidget(this.props);
  }

  componentWillUpdate() {
    return this.clearConditionalPanel();
  }

  componentDidUpdate(prevProps: Props) {
    this.keepFocusOnInput();
  }

  componentWillUnmount() {
    // This is called if CodeMirror is re-initializing itself before the
    // user closes the conditional panel. Clear the widget, and re-render it
    // as soon as this component gets remounted
    return this.clearConditionalPanel();
  }

  renderToWidget(props: Props) {
    if (this.cbPanel) {
      this.clearConditionalPanel();
    }

    const { location, editor } = props;

    const editorLine = toEditorLine(location.sourceId, location.line || 0);
    this.cbPanel = editor.codeMirror.addLineWidget(
      editorLine,
      this.renderConditionalPanel(props),
      {
        coverGutter: true,
        noHScroll: true,
      }
    );

    if (this.input) {
      let parent: ?Node = this.input.parentNode;
      while (parent) {
        if (
          parent instanceof HTMLElement &&
          parent.classList.contains("CodeMirror-scroll")
        ) {
          this.scrollParent = parent;
          break;
        }
        parent = (parent.parentNode: ?Node);
      }

      if (this.scrollParent) {
        this.scrollParent.addEventListener("scroll", this.repositionOnScroll);
        this.repositionOnScroll();
      }
    }
  }

  createEditor = (input: ?HTMLTextAreaElement) => {
    const { log, editor, closeConditionalPanel } = this.props;

    const codeMirror = editor.CodeMirror.fromTextArea(input, {
      mode: "javascript",
      theme: "mozilla",
      placeholder: L10N.getStr(
        log
          ? "editor.conditionalPanel.logPoint.placeholder2"
          : "editor.conditionalPanel.placeholder2"
      ),
    });

    codeMirror.on("keydown", (cm, e) => {
      if (e.key === "Enter") {
        e.codemirrorIgnore = true;
      }
    });

    codeMirror.on("blur", (cm, e) => closeConditionalPanel());

    const codeMirrorWrapper = codeMirror.getWrapperElement();

    codeMirrorWrapper.addEventListener("keydown", e => {
      codeMirror.save();
      this.onKey(e);
    });

    this.input = input;
    this.codeMirror = codeMirror;
    codeMirror.focus();
    codeMirror.setCursor(codeMirror.lineCount(), 0);
  };

  getDefaultValue() {
    const { breakpoint, log } = this.props;
    const options = (breakpoint && breakpoint.options) || {};
    return log ? options.logValue : options.condition;
  }

  renderConditionalPanel(props: Props) {
    const { log } = props;
    const defaultValue = this.getDefaultValue();

    const panel = document.createElement("div");
    ReactDOM.render(
      <div
        className={classNames("conditional-breakpoint-panel", {
          "log-point": log,
        })}
        onClick={() => this.keepFocusOnInput()}
        ref={node => (this.panelNode = node)}
      >
        <div className="prompt">»</div>
        <textarea
          defaultValue={defaultValue}
          ref={input => this.createEditor(input)}
        />
      </div>,
      panel
    );
    return panel;
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const location = getConditionalPanelLocation(state);
  return {
    cx: getContext(state),
    breakpoint: getBreakpoint(state, location),
    location,
    log: getLogPointStatus(state),
  };
};

const {
  setBreakpointOptions,
  openConditionalPanel,
  closeConditionalPanel,
} = actions;

const mapDispatchToProps = {
  setBreakpointOptions,
  openConditionalPanel,
  closeConditionalPanel,
};

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(ConditionalPanel);
