/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "../../../utils/connect";
import { isTesting } from "devtools-environment";

import Reps from "devtools-reps";
const {
  REPS: { Rep },
  MODE,
  objectInspector,
} = Reps;

const { ObjectInspector, utils } = objectInspector;

const {
  node: { nodeIsPrimitive, nodeIsFunction, nodeIsObject },
} = utils;

import actions from "../../../actions";
import { getThreadContext, getPreview } from "../../../selectors";
import Popover from "../../shared/Popover";
import PreviewFunction from "../../shared/PreviewFunction";

import { createObjectClient } from "../../../client/firefox";

import "./Popup.css";

import type { Coords } from "../../shared/Popover";
import type { ThreadContext } from "../../../types";
import type { Preview } from "../../../reducers/types";

type Props = {
  cx: ThreadContext,
  preview: Preview,
  editor: any,
  editorRef: ?HTMLDivElement,
  addExpression: typeof actions.addExpression,
  selectSourceURL: typeof actions.selectSourceURL,
  openLink: typeof actions.openLink,
  openElementInInspector: typeof actions.openElementInInspectorCommand,
  highlightDomElement: typeof actions.highlightDomElement,
  unHighlightDomElement: typeof actions.unHighlightDomElement,
  clearPreview: typeof actions.clearPreview,
};

type State = {
  top: number,
};

export class Popup extends Component<Props, State> {
  marker: any;
  pos: any;
  popup: ?HTMLDivElement;
  timerId: ?IntervalID;

  constructor(props: Props) {
    super(props);
    this.state = {
      top: 0,
    };
  }

  componentDidMount() {
    this.startTimer();
  }

  componentWillUnmount() {
    if (this.timerId) {
      clearInterval(this.timerId);
    }
  }

  startTimer() {
    this.timerId = setInterval(this.onInterval, 300);
  }

  onInterval = () => {
    const { preview, clearPreview, cx } = this.props;

    // Don't clear the current preview if mouse is hovered on
    // the current preview's element (target) or the popup element
    // Note, we disregard while testing because it is impossible to hover
    const currentTarget = preview.target;
    if (
      isTesting() ||
      currentTarget.matches(":hover") ||
      (this.popup && this.popup.matches(":hover"))
    ) {
      return;
    }

    // Clear the interval and the preview if it is not hovered
    // on the current preview's element or the popup element
    clearInterval(this.timerId);
    return clearPreview(cx);
  };

  calculateMaxHeight = () => {
    const { editorRef } = this.props;
    if (!editorRef) {
      return "auto";
    }
    return editorRef.getBoundingClientRect().height - this.state.top;
  };

  renderFunctionPreview() {
    const {
      cx,
      selectSourceURL,
      preview: { result },
    } = this.props;

    return (
      <div
        className="preview-popup"
        ref={a => (this.popup = a)}
        onClick={() =>
          selectSourceURL(cx, result.location.url, {
            line: result.location.line,
          })
        }
      >
        <PreviewFunction func={result} />
      </div>
    );
  }

  renderObjectPreview() {
    const {
      preview: { properties },
      openLink,
      openElementInInspector,
      highlightDomElement,
      unHighlightDomElement,
    } = this.props;

    return (
      <div
        className="preview-popup"
        style={{ maxHeight: this.calculateMaxHeight() }}
        ref={a => (this.popup = a)}
      >
        <ObjectInspector
          roots={properties}
          autoExpandDepth={0}
          disableWrap={true}
          focusable={false}
          openLink={openLink}
          createObjectClient={grip => createObjectClient(grip)}
          onDOMNodeClick={grip => openElementInInspector(grip)}
          onInspectIconClick={grip => openElementInInspector(grip)}
          onDOMNodeMouseOver={grip => highlightDomElement(grip)}
          onDOMNodeMouseOut={grip => unHighlightDomElement(grip)}
        />
      </div>
    );
  }

  renderSimplePreview() {
    const {
      openLink,
      preview: { result },
    } = this.props;
    return (
      <div className="preview-popup" ref={a => (this.popup = a)}>
        {Rep({
          object: result,
          mode: MODE.LONG,
          openLink,
        })}
      </div>
    );
  }

  renderPreview() {
    // We don't have to check and
    // return on `false`, `""`, `0`, `undefined` etc,
    // these falsy simple typed value because we want to
    // do `renderSimplePreview` on these values below.
    const {
      preview: { root },
    } = this.props;

    if (nodeIsFunction(root)) {
      return this.renderFunctionPreview();
    }

    if (nodeIsObject(root)) {
      return <div>{this.renderObjectPreview()}</div>;
    }

    return this.renderSimplePreview();
  }

  getPreviewType() {
    const {
      preview: { root },
    } = this.props;
    if (nodeIsPrimitive(root) || nodeIsFunction(root)) {
      return "tooltip";
    }

    return "popover";
  }

  onPopoverCoords = (coords: Coords) => {
    this.setState({ top: coords.top });
  };

  render() {
    const {
      preview: { cursorPos, result },
      editorRef,
    } = this.props;
    const type = this.getPreviewType();

    if (typeof result == "undefined" || result.optimizedOut) {
      return null;
    }

    return (
      <Popover
        targetPosition={cursorPos}
        type={type}
        onPopoverCoords={this.onPopoverCoords}
        editorRef={editorRef}
      >
        {this.renderPreview()}
      </Popover>
    );
  }
}

const mapStateToProps = state => ({
  cx: getThreadContext(state),
  preview: getPreview(state),
});

const {
  addExpression,
  selectSourceURL,
  openLink,
  openElementInInspectorCommand,
  highlightDomElement,
  unHighlightDomElement,
  clearPreview,
} = actions;

const mapDispatchToProps = {
  addExpression,
  selectSourceURL,
  openLink,
  openElementInInspector: openElementInInspectorCommand,
  highlightDomElement,
  unHighlightDomElement,
  clearPreview,
};

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(Popup);
