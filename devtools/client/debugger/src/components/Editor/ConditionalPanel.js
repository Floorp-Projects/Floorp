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
  getBreakpointForLocation,
  getConditionalPanelLocation,
  getLogPointStatus,
  getContext
} from "../../selectors";

import type { SourceLocation, Context } from "../../types";

type Props = {
  cx: Context,
  breakpoint: ?Object,
  setBreakpointOptions: typeof actions.setBreakpointOptions,
  location: SourceLocation,
  log: boolean,
  editor: Object,
  openConditionalPanel: typeof actions.openConditionalPanel,
  closeConditionalPanel: typeof actions.closeConditionalPanel
};

export class ConditionalPanel extends PureComponent<Props> {
  cbPanel: null | Object;
  input: ?HTMLInputElement;
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
      this.setBreakpoint(this.input.value);
    }

    this.props.closeConditionalPanel();
  };

  onKey = (e: SyntheticKeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      this.saveAndClose();
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
      [type]: value
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
        noHScroll: true
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

  renderConditionalPanel(props: Props) {
    const { breakpoint, log, editor } = props;
    const options = (breakpoint && breakpoint.options) || {};
    const condition = log ? options.logValue : options.condition;

    const panel = document.createElement("div");
    ReactDOM.render(
      <div
        className={classNames("conditional-breakpoint-panel", {
          "log-point": log
        })}
        onClick={() => this.keepFocusOnInput()}
        onBlur={this.props.closeConditionalPanel}
        ref={node => (this.panelNode = node)}
      >
        <div className="prompt">»</div>
        <input
          defaultValue={condition}
          ref={input => {
            const codeMirror = editor.CodeMirror.fromTextArea(input, {
              mode: "javascript",
              theme: "mozilla",
              placeholder: L10N.getStr(
                log
                  ? "editor.conditionalPanel.logPoint.placeholder"
                  : "editor.conditionalPanel.placeholder"
              )
            });
            const codeMirrorWrapper = codeMirror.getWrapperElement();

            codeMirrorWrapper.addEventListener("keydown", e => {
              codeMirror.save();
              this.onKey(e);
            });

            this.input = input;
            codeMirror.focus();
            codeMirror.setCursor(codeMirror.lineCount(), 0);
          }}
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
  const log = getLogPointStatus(state);
  return {
    cx: getContext(state),
    breakpoint: getBreakpointForLocation(state, location),
    location,
    log
  };
};

const {
  setBreakpointOptions,
  openConditionalPanel,
  closeConditionalPanel
} = actions;

const mapDispatchToProps = {
  setBreakpointOptions,
  openConditionalPanel,
  closeConditionalPanel
};

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(ConditionalPanel);
