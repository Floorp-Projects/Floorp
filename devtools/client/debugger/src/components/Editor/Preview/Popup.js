/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import { div } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../../utils/connect";

import Reps from "devtools/client/shared/components/reps/index";
const {
  REPS: { Grip },
  MODE,
  objectInspector,
} = Reps;

const { ObjectInspector, utils } = objectInspector;

const {
  node: { nodeIsPrimitive },
} = utils;

import ExceptionPopup from "./ExceptionPopup";

import actions from "../../../actions";
import Popover from "../../shared/Popover";

import "./Popup.css";

export class Popup extends Component {
  constructor(props) {
    super(props);
  }

  static get propTypes() {
    return {
      clearPreview: PropTypes.func.isRequired,
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

  renderExceptionPreview(exception) {
    return React.createElement(ExceptionPopup, {
      exception: exception,
      clearPreview: this.props.clearPreview,
    });
  }

  renderPreview() {
    const {
      preview: { root, exception, resultGrip },
    } = this.props;

    const usesCustomFormatter =
      root?.contents?.value?.useCustomFormatter ?? false;

    if (exception) {
      return this.renderExceptionPreview(exception);
    }

    return div(
      {
        className: "preview-popup",
        style: {
          maxHeight: this.calculateMaxHeight(),
        },
      },
      React.createElement(ObjectInspector, {
        roots: [root],
        autoExpandDepth: 1,
        autoReleaseObjectActors: false,
        mode: usesCustomFormatter ? MODE.LONG : MODE.SHORT,
        disableWrap: true,
        displayRootNodeAsHeader: true,
        focusable: false,
        openLink: this.props.openLink,
        defaultRep: Grip,
        createElement: this.createElement,
        onDOMNodeClick: grip => this.props.openElementInInspector(grip),
        onInspectIconClick: grip => this.props.openElementInInspector(grip),
        onDOMNodeMouseOver: grip => this.props.highlightDomElement(grip),
        onDOMNodeMouseOut: grip => this.props.unHighlightDomElement(grip),
        mayUseCustomFormatter: true,
        onViewSourceInDebugger: () => {
          return (
            resultGrip.location &&
            this.props.selectSourceURL(resultGrip.location.url, {
              line: resultGrip.location.line,
              column: resultGrip.location.column,
            })
          );
        },
      })
    );
  }

  getPreviewType() {
    const {
      preview: { root, exception },
    } = this.props;
    if (exception || nodeIsPrimitive(root)) {
      return "tooltip";
    }

    return "popover";
  }

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
    return React.createElement(
      Popover,
      {
        targetPosition: cursorPos,
        type: type,
        editorRef: editorRef,
        target: this.props.preview.target,
        mouseout: this.props.clearPreview,
      },
      this.renderPreview()
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

const mapDispatchToProps = {
  addExpression: actions.addExpression,
  selectSourceURL: actions.selectSourceURL,
  openLink: actions.openLink,
  openElementInInspector: actions.openElementInInspectorCommand,
  highlightDomElement: actions.highlightDomElement,
  unHighlightDomElement: actions.unHighlightDomElement,
};

export default connect(null, mapDispatchToProps)(Popup);
