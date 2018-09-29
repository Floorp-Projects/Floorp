/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

loader.lazyGetter(this, "Rep", function() {
  return require("devtools/client/shared/components/reps/reps").REPS.Rep;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

loader.lazyRequireGetter(this, "translateNodeFrontToGrip", "devtools/client/inspector/shared/utils", true);

class ComputedProperty extends PureComponent {
  static get propTypes() {
    return {
      name: PropTypes.string.isRequired,
      onHideBoxModelHighlighter: PropTypes.func,
      onShowBoxModelHighlighterForNode: PropTypes.func,
      referenceElement: PropTypes.object,
      referenceElementType: PropTypes.string,
      setSelectedNode: PropTypes.func,
      value: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.onFocus = this.onFocus.bind(this);
    this.renderReferenceElementPreview = this.renderReferenceElementPreview.bind(this);
  }

  onFocus() {
    this.container.focus();
  }

  renderReferenceElementPreview() {
    const {
      onShowBoxModelHighlighterForNode,
      onHideBoxModelHighlighter,
      referenceElement,
      referenceElementType,
      setSelectedNode,
    } = this.props;

    if (!referenceElement) {
      return null;
    }

    return (
      dom.div({ className: "reference-element" },
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
      )
    );
  }

  render() {
    const { name, value } = this.props;

    return (
      dom.div(
        {
          className: "computed-property-view",
          "data-property-name": name,
          tabIndex: "0",
          ref: container => {
            this.container = container;
          },
        },
        dom.div({ className: "computed-property-name-container" },
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
        dom.div({ className: "computed-property-value-container" },
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
      )
    );
  }
}

module.exports = ComputedProperty;
