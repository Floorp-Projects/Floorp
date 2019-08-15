/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "../../../utils/connect";

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

export class Popup extends Component<Props> {
  marker: any;
  pos: any;
  popover: ?React$ElementRef<typeof Popover>;

  constructor(props: Props) {
    super(props);
  }

  componentDidMount() {
    this.addHighlightToToken();
  }

  componentWillUnmount() {
    this.removeHighlightFromToken();
  }

  addHighlightToToken() {
    const target = this.props.preview.target;
    if (target) {
      target.classList.add("preview-token");
      addHighlightToTargetSiblings(target, this.props);
    }
  }

  removeHighlightFromToken() {
    const target = this.props.preview.target;
    if (target) {
      target.classList.remove("preview-token");
      removeHighlightForTargetSiblings(target);
    }
  }

  calculateMaxHeight = () => {
    const { editorRef } = this.props;
    if (!editorRef) {
      return "auto";
    }

    const { height, top } = editorRef.getBoundingClientRect();
    const maxHeight = height + top;
    if (maxHeight < 250) {
      return maxHeight;
    }

    return 250;
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
      <div className="preview-popup">
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

  onMouseOut = () => {
    const { clearPreview, cx } = this.props;
    clearPreview(cx);
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
        editorRef={editorRef}
        target={this.props.preview.target}
        mouseout={this.onMouseOut}
      >
        {this.renderPreview()}
      </Popover>
    );
  }
}

function addHighlightToTargetSiblings(target: Element, props: Object) {
  // Look at target's pervious and next token siblings.
  // If they are the same token type, and are also found in the preview expression,
  // add the highlight class to them as well.

  const tokenType = target.classList.item(0);
  const previewExpression = props.preview.expression;

  if (
    tokenType &&
    previewExpression &&
    target.innerHTML !== previewExpression
  ) {
    let nextSibling = target.nextElementSibling;
    while (
      nextSibling &&
      nextSibling.className.includes(tokenType) &&
      previewExpression.includes(nextSibling.innerHTML)
    ) {
      nextSibling.classList.add("preview-token");
      nextSibling = nextSibling.nextElementSibling;
    }
    let previousSibling = target.previousElementSibling;
    while (
      previousSibling &&
      previousSibling.className.includes(tokenType) &&
      previewExpression.includes(previousSibling.innerHTML)
    ) {
      previousSibling.classList.add("preview-token");
      previousSibling = previousSibling.previousElementSibling;
    }
  }
}

function removeHighlightForTargetSiblings(target: Element) {
  // Look at target's previous and next token siblings.
  // If they also have the highlight class 'preview-token',
  // remove that class.
  let nextSibling = target.nextElementSibling;
  while (nextSibling && nextSibling.className.includes("preview-token")) {
    nextSibling.classList.remove("preview-token");
    nextSibling = nextSibling.nextElementSibling;
  }
  let previousSibling = target.previousElementSibling;
  while (
    previousSibling &&
    previousSibling.className.includes("preview-token")
  ) {
    previousSibling.classList.remove("preview-token");
    previousSibling = previousSibling.previousElementSibling;
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
