/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createRef,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { editableItem } = require("devtools/client/shared/inplace-editor");

const { getFormatStr } = require("../utils/l10n");

class TextNode extends PureComponent {
  static get propTypes() {
    return {
      showTextEditor: PropTypes.func.isRequired,
      type: PropTypes.string.isRequired,
      value: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      value: this.props.value,
    };

    this.valuePreRef = createRef();
  }

  componentDidMount() {
    editableItem(
      {
        element: this.valuePreRef.current,
        trigger: "dblclick",
      },
      element => {
        this.props.showTextEditor(element);
      }
    );
  }

  render() {
    const { value } = this.state;
    const isComment = this.props.type === "comment";
    const isWhiteSpace = !/[^\s]/.exec(value);

    return createElement(
      Fragment,
      null,
      isComment ? dom.span({}, "<!--") : null,
      dom.pre(
        {
          className: isWhiteSpace ? "whitespace" : "",
          ref: this.valuePreRef,
          style: {
            display: "inline-block",
            whiteSpace: "normal",
          },
          tabIndex: -1,
          title: isWhiteSpace
            ? getFormatStr(
                "markupView.whitespaceOnly",
                value
                  .replace(/\n/g, "⏎")
                  .replace(/\t/g, "⇥")
                  .replace(/ /g, "◦")
              )
            : "",
        },
        value
      ),
      isComment ? dom.span({}, "-->") : null
    );
  }
}

module.exports = TextNode;
