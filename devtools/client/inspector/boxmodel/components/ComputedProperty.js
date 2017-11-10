/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DOM: dom,
  PropTypes,
  PureComponent,
} = require("devtools/client/shared/vendor/react");

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
    this.translateNodeFrontToGrip = this.translateNodeFrontToGrip.bind(this);
    this.onFocus = this.onFocus.bind(this);
  }

  renderReferenceElementPreview() {
    let {
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
        object: this.translateNodeFrontToGrip(referenceElement),
        onInspectIconClick: () => setSelectedNode(referenceElement, "box-model"),
        onDOMNodeMouseOver: () => onShowBoxModelHighlighterForNode(referenceElement),
        onDOMNodeMouseOut: () => onHideBoxModelHighlighter(),
      })
    );
  }

  /**
   * While waiting for a reps fix in https://github.com/devtools-html/reps/issues/92,
   * translate nodeFront to a grip-like object that can be used with an ElementNode rep.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we want to create a grip-like object.
   * @return {Object} a grip-like object that can be used with Reps.
   */
  translateNodeFrontToGrip(nodeFront) {
    let {
      attributes
    } = nodeFront;

    // The main difference between NodeFront and grips is that attributes are treated as
    // a map in grips and as an array in NodeFronts.
    let attributesMap = {};
    for (let { name, value } of attributes) {
      attributesMap[name] = value;
    }

    return {
      actor: nodeFront.actorID,
      preview: {
        attributes: attributesMap,
        attributesLength: attributes.length,
        // nodeName is already lowerCased in Node grips
        nodeName: nodeFront.nodeName.toLowerCase(),
        nodeType: nodeFront.nodeType,
        isConnected: true,
      }
    };
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
