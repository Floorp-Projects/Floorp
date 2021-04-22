/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { gDevTools } = require("devtools/client/framework/devtools");
const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

const {
  div,
  img,
} = require("devtools/client/shared/vendor/react-dom-factories");

const { getColor } = require("devtools/client/shared/theme");

const FONT_NAME = L10N.getStr("netmonitor.response.name");
const FONT_MIME_TYPE = L10N.getStr("netmonitor.response.mime");
const FONT_PREVIEW_FAILED = L10N.getStr(
  "netmonitor.response.fontPreviewFailed"
);

const FONT_PREVIEW_TEXT =
  "ABCDEFGHIJKLM\nNOPQRSTUVWXYZ\nabcdefghijklm\nnopqrstuvwxyz\n0123456789";

class FontPreview extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      mimeType: PropTypes.string,
      url: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      name: "",
      dataURL: "",
    };

    this.onThemeChanged = this.onThemeChanged.bind(this);
  }

  componentDidMount() {
    this.getPreview();

    // Listen for theme changes as the color of the preview depends on the theme
    gDevTools.on("theme-switched", this.onThemeChanged);
  }

  componentDidUpdate(prevProps) {
    const { url } = this.props;
    if (prevProps.url !== url) {
      this.getPreview();
    }
  }

  componentWillUnmount() {
    gDevTools.off("theme-switched", this.onThemeChanged);
  }

  /**
   * Handler for the "theme-switched" event.
   */
  onThemeChanged(frame) {
    if (frame === window) {
      this.getPreview();
    }
  }

  /**
   * Generate the font preview and receives information about the font.
   */
  async getPreview() {
    const { connector } = this.props;

    const toolbox = connector.getToolbox();
    const inspectorFront = await toolbox.target.getFront("inspector");
    const { pageStyle } = inspectorFront;
    const pageFontFaces = await pageStyle.getAllUsedFontFaces({
      includePreviews: true,
      includeVariations: false,
      previewText: FONT_PREVIEW_TEXT,
      previewFillStyle: getColor("body-color"),
    });

    const fontFace = pageFontFaces.find(
      pageFontFace => pageFontFace.URI === this.props.url
    );

    this.setState({
      name: fontFace?.name ?? "",
      dataURL: (await fontFace?.preview.data.string()) ?? "",
    });
  }

  render() {
    const { mimeType } = this.props;
    const { name, dataURL } = this.state;

    if (dataURL === "") {
      return div({ className: "empty-notice" }, FONT_PREVIEW_FAILED);
    }

    return div(
      { className: "panel-container response-font-box devtools-monospace" },
      img({
        className: "response-font",
        src: dataURL,
        alt: "",
      }),
      div(
        { className: "response-summary" },
        div({ className: "tabpanel-summary-label" }, FONT_NAME),
        div({ className: "tabpanel-summary-value" }, name)
      ),
      div(
        { className: "response-summary" },
        div({ className: "tabpanel-summary-label" }, FONT_MIME_TYPE),
        div({ className: "tabpanel-summary-value" }, mimeType)
      )
    );
  }
}

module.exports = FontPreview;
