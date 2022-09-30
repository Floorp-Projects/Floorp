/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");

const {
  div,
  img,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const {
  formDataURI,
  getUrlBaseName,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

const RESPONSE_IMG_NAME = L10N.getStr("netmonitor.response.name");
const RESPONSE_IMG_DIMENSIONS = L10N.getStr("netmonitor.response.dimensions");
const RESPONSE_IMG_MIMETYPE = L10N.getStr("netmonitor.response.mime");

class ImagePreview extends Component {
  static get propTypes() {
    return {
      mimeType: PropTypes.string,
      encoding: PropTypes.string,
      text: PropTypes.string,
      url: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      dimensions: {
        width: 0,
        height: 0,
      },
    };

    this.updateDimensions = this.updateDimensions.bind(this);
  }

  updateDimensions({ target }) {
    this.setState({
      dimensions: {
        width: target.naturalWidth,
        height: target.naturalHeight,
      },
    });
  }

  render() {
    const { mimeType, encoding, text, url } = this.props;
    const { width, height } = this.state.dimensions;

    return div(
      { className: "panel-container response-image-box devtools-monospace" },
      img({
        className: "response-image",
        src: formDataURI(mimeType, encoding, text),
        onLoad: this.updateDimensions,
      }),
      div(
        { className: "response-summary" },
        div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_NAME),
        div({ className: "tabpanel-summary-value" }, getUrlBaseName(url))
      ),
      div(
        { className: "response-summary" },
        div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_DIMENSIONS),
        div({ className: "tabpanel-summary-value" }, `${width} × ${height}`)
      ),
      div(
        { className: "response-summary" },
        div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_MIMETYPE),
        div({ className: "tabpanel-summary-value" }, mimeType)
      )
    );
  }
}

module.exports = ImagePreview;
