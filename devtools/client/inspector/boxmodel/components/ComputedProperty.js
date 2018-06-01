/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { translateNodeFrontToGrip } = require("devtools/client/inspector/shared/utils");

const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;

class ComputedProperty extends PureComponent {
  static get propTypes() {
    return {
      name: PropTypes.string.isRequired,
      referenceElement: PropTypes.object,
      referenceElementType: PropTypes.string,
      setSelectedNode: PropTypes.func.isRequired,
      value: PropTypes.string,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.renderReferenceElementPreview = this.renderReferenceElementPreview.bind(this);
    this.onFocus = this.onFocus.bind(this);
  }

  renderReferenceElementPreview() {
    const {
      referenceElement,
      referenceElementType,
      setSelectedNode,
      onShowBoxModelHighlighterForNode,
      onHideBoxModelHighlighter
    } = this.props;

    if (!referenceElement) {
      return null;
    }

    return dom.div(
      {
        className: "reference-element"
      },
      dom.span({ className: "reference-element-type" }, referenceElementType),
      Rep({
        defaultRep: referenceElement,
        mode: MODE.TINY,
        object: translateNodeFrontToGrip(referenceElement),
        onInspectIconClick: () => setSelectedNode(referenceElement,
          { reason: "box-model" }),
        onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(referenceElement),
        onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
      })
    );
  }

  onFocus() {
    this.container.focus();
  }

  render() {
    const { name, value } = this.props;

    return dom.div(
      {
        className: "computed-property-view",
        "data-property-name": name,
        tabIndex: "0",
        ref: container => {
          this.container = container;
        },
      },
      dom.div(
        {
          className: "computed-property-name-container",
        },
        dom.div(
          {
            className: "computed-property-name theme-fg-color5",
            tabIndex: "",
            title: name,
            onClick: this.onFocus,
          },
          name
        )
      ),
      dom.div(
        {
          className: "computed-property-value-container",
        },
        dom.div(
          {
            className: "computed-property-value theme-fg-color1",
            dir: "ltr",
            tabIndex: "",
            onClick: this.onFocus,
          },
          value
        ),
        this.renderReferenceElementPreview()
      )
    );
  }
}

module.exports = ComputedProperty;
