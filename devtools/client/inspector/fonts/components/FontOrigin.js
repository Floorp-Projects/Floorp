/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

loader.lazyRequireGetter(
  this,
  "clipboardHelper",
  "devtools/shared/platform/clipboard"
);

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class FontOrigin extends PureComponent {
  static get propTypes() {
    return {
      font: PropTypes.shape(Types.font).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onCopyURL = this.onCopyURL.bind(this);
  }

  onCopyURL() {
    clipboardHelper.copyString(this.props.font.URI);
  }

  render() {
    const url = this.props.font.URI;

    if (!url) {
      return dom.p(
        {
          className: "font-origin system",
        },
        getStr("fontinspector.system")
      );
    }

    return dom.p(
      {
        className: "font-origin remote",
      },
      dom.span(
        {
          className: "url",
          title: url,
        },
        url
      ),
      dom.button({
        className: "copy-icon",
        onClick: this.onCopyURL,
        title: getStr("fontinspector.copyURL"),
      })
    );
  }
}

module.exports = FontOrigin;
