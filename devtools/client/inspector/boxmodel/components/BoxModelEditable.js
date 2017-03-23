/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { editableItem } = require("devtools/client/shared/inplace-editor");

const LONG_TEXT_ROTATE_LIMIT = 3;

module.exports = createClass({

  displayName: "BoxModelEditable",

  propTypes: {
    box: PropTypes.string.isRequired,
    direction: PropTypes.string,
    property: PropTypes.string.isRequired,
    textContent: PropTypes.oneOfType([PropTypes.string, PropTypes.number]).isRequired,
    onShowBoxModelEditor: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  componentDidMount() {
    let { property, onShowBoxModelEditor } = this.props;

    editableItem({
      element: this.refs.span,
    }, (element, event) => {
      onShowBoxModelEditor(element, event, property);
    });
  },

  render() {
    let {
      box,
      direction,
      property,
      textContent,
    } = this.props;

    let rotate = direction &&
                 (direction == "left" || direction == "right") &&
                 textContent.toString().length > LONG_TEXT_ROTATE_LIMIT;

    return dom.p(
      {
        className: `boxmodel-${box}
                    ${direction ? " boxmodel-" + direction : "boxmodel-" + property}
                    ${rotate ? " boxmodel-rotate" : ""}`,
      },
      dom.span(
        {
          className: "boxmodel-editable",
          "data-box": box,
          title: property,
          ref: "span",
        },
        textContent
      )
    );
  },

});
