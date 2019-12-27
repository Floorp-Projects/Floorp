/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");
const { getStr } = require("../utils/l10n");

const PREVIEW_TEXT_MAX_LENGTH = 30;

class FontPreviewInput extends PureComponent {
  static get propTypes() {
    return {
      onPreviewTextChange: PropTypes.func.isRequired,
      previewText: Types.fontOptions.previewText.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onChange = this.onChange.bind(this);
    this.onFocus = this.onFocus.bind(this);
    this.inputRef = createRef();

    this.state = {
      value: this.props.previewText,
    };
  }

  onChange(e) {
    const value = e.target.value;
    this.props.onPreviewTextChange(value);

    this.setState(prevState => {
      return { ...prevState, value };
    });
  }

  onFocus(e) {
    e.target.select();
  }

  focus() {
    this.inputRef.current.focus();
  }

  render() {
    return dom.div(
      {
        id: "font-preview-input-container",
      },
      dom.input({
        className: "devtools-searchinput",
        onChange: this.onChange,
        onFocus: this.onFocus,
        maxLength: PREVIEW_TEXT_MAX_LENGTH,
        placeholder: getStr("fontinspector.previewTextPlaceholder"),
        ref: this.inputRef,
        type: "text",
        value: this.state.value,
      })
    );
  }
}

module.exports = FontPreviewInput;
