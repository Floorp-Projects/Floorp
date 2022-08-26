/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";

import Reps from "devtools/client/shared/components/reps/index";
const {
  REPS: { Rep },
  MODE,
  objectInspector,
} = Reps;

const { ObjectInspector, utils } = objectInspector;

const {
  node: { nodeIsPrimitive, nodeIsFunction, nodeIsObject },
} = utils;

import ExceptionPopup from "./ExceptionPopup";

import actions from "../../../actions";
import { getThreadContext } from "../../../selectors";
import Popover from "../../shared/Popover";
import PreviewFunction from "../../shared/PreviewFunction";

import "./Popup.css";

export class Popup extends Component {
  constructor(props) {
    super(props);
  }

  static get propTypes() {
    return {
      clearPreview: PropTypes.func.isRequired,
      cx: PropTypes.object.isRequired,
      editorRef: PropTypes.object.isRequired,
      highlightDomElement: PropTypes.func.isRequired,
      openElementInInspector: PropTypes.func.isRequired,
      openLink: PropTypes.func.isRequired,
      preview: PropTypes.object.isRequired,
      selectSourceURL: PropTypes.func.isRequired,
      unHighlightDomElement: PropTypes.func.isRequired,
    };
  }

  componentDidMount() {
    this.addHighlightToToken();
  }

  componentWillUnmount() {
    this.removeHighlightFromToken();
  }

  addHighlightToToken() {
    const { target } = this.props.preview;
    if (target) {
      target.classList.add("preview-token");
      addHighlightToTargetSiblings(target, this.props);
    }
  }

  removeHighlightFromToken() {
    const { target } = this.props.preview;
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

  createElement(element) {
    return document.createElement(element);
  }

  renderFunctionPreview() {
    const {
      cx,
      selectSourceURL,
      preview: { resultGrip },
    } = this.props;

    if (!resultGrip) {
      return null;
    }

    const { location } = resultGrip;

    return (
      <div
        className="preview-popup"
        onClick={() =>
          location &&
          selectSourceURL(cx, location.url, {
            line: location.line,
          })
        }
      >
        <PreviewFunction func={resultGrip} />
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

    if (!properties.length) {
      return (
        <div className="preview-popup">
          <span className="label">{L10N.getStr("preview.noProperties")}</span>
        </div>
      );
    }

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
          createElement={this.createElement}
          onDOMNodeClick={grip => openElementInInspector(grip)}
          onInspectIconClick={grip => openElementInInspector(grip)}
          onDOMNodeMouseOver={grip => highlightDomElement(grip)}
          onDOMNodeMouseOut={grip => unHighlightDomElement(grip)}
          mayUseCustomFormatter={true}
        />
      </div>
    );
  }

  renderSimplePreview() {
    const {
      openLink,
      preview: { resultGrip },
    } = this.props;
    return (
      <div className="preview-popup">
        {Rep({
          object: resultGrip,
          mode: MODE.LONG,
          openLink,
        })}
      </div>
    );
  }

  renderExceptionPreview(exception) {
    return (
      <ExceptionPopup
        exception={exception}
        mouseout={this.onMouseOutException}
      />
    );
  }

  renderPreview() {
    // We don't have to check and
    // return on `false`, `""`, `0`, `undefined` etc,
    // these falsy simple typed value because we want to
    // do `renderSimplePreview` on these values below.
    const {
      preview: { root, exception },
    } = this.props;

    if (nodeIsFunction(root)) {
      return this.renderFunctionPreview();
    }

    if (nodeIsObject(root)) {
      return <div>{this.renderObjectPreview()}</div>;
    }

    if (exception) {
      return this.renderExceptionPreview(exception);
    }

    return this.renderSimplePreview();
  }

  getPreviewType() {
    const {
      preview: { root, properties, exception },
    } = this.props;
    if (
      exception ||
      nodeIsPrimitive(root) ||
      nodeIsFunction(root) ||
      !Array.isArray(properties) ||
      properties.length === 0
    ) {
      return "tooltip";
    }

    return "popover";
  }

  onMouseOut = () => {
    const { clearPreview, cx } = this.props;

    clearPreview(cx);
  };

  onMouseOutException = (shouldClearOnMouseout, isExceptionStactraceOpen) => {
    // onMouseOutException can be called:
    // a. when the mouse leaves Popover element
    // b. when the mouse leaves ExceptionPopup element
    // We want to prevent closing the popup when the stacktrace
    // is expanded and the mouse leaves either the Popover element
    // or the ExceptionPopup element.
    const { clearPreview, cx } = this.props;

    if (shouldClearOnMouseout) {
      this.isExceptionStactraceOpen = isExceptionStactraceOpen;
    }

    if (!this.isExceptionStactraceOpen) {
      clearPreview(cx);
    }
  };

  render() {
    const {
      preview: { cursorPos, resultGrip, exception },
      editorRef,
    } = this.props;

    if (
      !exception &&
      (typeof resultGrip == "undefined" || resultGrip?.optimizedOut)
    ) {
      return null;
    }

    const type = this.getPreviewType();
    return (
      <Popover
        targetPosition={cursorPos}
        type={type}
        editorRef={editorRef}
        target={this.props.preview.target}
        mouseout={exception ? this.onMouseOutException : this.onMouseOut}
      >
        {this.renderPreview()}
      </Popover>
    );
  }
}

export function addHighlightToTargetSiblings(target, props) {
  // This function searches for related tokens that should also be highlighted when previewed.
  // Here is the process:
  // It conducts a search on the target's next siblings and then another search for the previous siblings.
  // If a sibling is not an element node (nodeType === 1), the highlight is not added and the search is short-circuited.
  // If the element sibling is the same token type as the target, and is also found in the preview expression, the highlight class is added.

  const tokenType = target.classList.item(0);
  const previewExpression = props.preview.expression;

  if (
    tokenType &&
    previewExpression &&
    target.innerHTML !== previewExpression
  ) {
    let nextSibling = target.nextSibling;
    let nextElementSibling = target.nextElementSibling;

    // Note: Declaring previous/next ELEMENT siblings as well because
    // properties like innerHTML can't be checked on nextSibling
    // without creating a flow error even if the node is an element type.
    while (
      nextSibling &&
      nextElementSibling &&
      nextSibling.nodeType === 1 &&
      nextElementSibling.className.includes(tokenType) &&
      previewExpression.includes(nextElementSibling.innerHTML)
    ) {
      // All checks passed, add highlight and continue the search.
      nextElementSibling.classList.add("preview-token");

      nextSibling = nextSibling.nextSibling;
      nextElementSibling = nextElementSibling.nextElementSibling;
    }

    let previousSibling = target.previousSibling;
    let previousElementSibling = target.previousElementSibling;

    while (
      previousSibling &&
      previousElementSibling &&
      previousSibling.nodeType === 1 &&
      previousElementSibling.className.includes(tokenType) &&
      previewExpression.includes(previousElementSibling.innerHTML)
    ) {
      // All checks passed, add highlight and continue the search.
      previousElementSibling.classList.add("preview-token");

      previousSibling = previousSibling.previousSibling;
      previousElementSibling = previousElementSibling.previousElementSibling;
    }
  }
}

export function removeHighlightForTargetSiblings(target) {
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

export default connect(mapStateToProps, mapDispatchToProps)(Popup);
