/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import { connect } from "react-redux";
import "./ConditionalPanel.css";
import { toEditorLine } from "../../utils/editor";
import actions from "../../actions";

import {
  getSelectedLocation,
  getBreakpointForLine,
  getConditionalPanelLine
} from "../../selectors";

type Props = {
  breakpoint: ?Object,
  selectedLocation: Object,
  setBreakpointCondition: Function,
  line: number,
  editor: Object,
  openConditionalPanel: () => void,
  closeConditionalPanel: () => void
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

  setBreakpoint(condition: string) {
    const { selectedLocation, line } = this.props;
    const sourceId = selectedLocation ? selectedLocation.sourceId : "";
    const location = { sourceId, line };
    return this.props.setBreakpointCondition(location, { condition });
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
    if (this.props.line) {
      return this.renderToWidget(this.props);
    }
  }

  componentWillUpdate(nextProps: Props) {
    if (nextProps.line) {
      return this.renderToWidget(nextProps);
    }
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
      if (this.props.line && this.props.line == props.line) {
        return props.closeConditionalPanel();
      }
      this.clearConditionalPanel();
    }

    const { selectedLocation, line, editor } = props;
    const sourceId = selectedLocation ? selectedLocation.sourceId : "";

    const editorLine = toEditorLine(sourceId, line);
    this.cbPanel = editor.codeMirror.addLineWidget(
      editorLine,
      this.renderConditionalPanel(props),
      {
        coverGutter: true,
        noHScroll: false
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
    const { breakpoint } = props;
    const condition = breakpoint ? breakpoint.condition : "";
    const panel = document.createElement("div");
    ReactDOM.render(
      <div
        className="conditional-breakpoint-panel"
        onClick={() => this.keepFocusOnInput()}
        onBlur={this.props.closeConditionalPanel}
        ref={node => (this.panelNode = node)}
      >
        <div className="prompt">»</div>
        <input
          defaultValue={condition}
          placeholder={L10N.getStr("editor.conditionalPanel.placeholder")}
          onKeyDown={this.onKey}
          ref={input => {
            this.input = input;
            this.keepFocusOnInput();
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
  const line = getConditionalPanelLine(state);
  const selectedLocation = getSelectedLocation(state);

  return {
    selectedLocation,
    breakpoint: getBreakpointForLine(state, selectedLocation.sourceId, line),
    line
  };
};

const {
  setBreakpointCondition,
  openConditionalPanel,
  closeConditionalPanel
} = actions;

const mapDispatchToProps = {
  setBreakpointCondition,
  openConditionalPanel,
  closeConditionalPanel
};

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(ConditionalPanel);
