/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { L10N } = require("../../l10n");
const { formDataURI, getUrlBaseName } = require("../../request-utils");

// Components
const PropertiesView = createFactory(require("./properties-view"));

const { div, img } = DOM;
const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const JSON_FILTER_TEXT = L10N.getStr("jsonFilterText");
const RESPONSE_IMG_NAME = L10N.getStr("netmonitor.response.name");
const RESPONSE_IMG_DIMENSIONS = L10N.getStr("netmonitor.response.dimensions");
const RESPONSE_IMG_MIMETYPE = L10N.getStr("netmonitor.response.mime");
const RESPONSE_PAYLOAD = L10N.getStr("responsePayload");

/*
 * Response panel component
 * Displays the GET parameters and POST data of a request
 */
const ResponsePanel = createClass({
  displayName: "ResponsePanel",

  propTypes: {
    request: PropTypes.object.isRequired,
  },

  getInitialState() {
    return {
      imageDimensions: {
        width: 0,
        height: 0,
      },
    };
  },

  updateImageDimemsions({ target }) {
    this.setState({
      imageDimensions: {
        width: target.naturalWidth,
        height: target.naturalHeight,
      },
    });
  },

  // Handle json, which we tentatively identify by checking the MIME type
  // for "json" after any word boundary. This works for the standard
  // "application/json", and also for custom types like "x-bigcorp-json".
  // Additionally, we also directly parse the response text content to
  // verify whether it's json or not, to handle responses incorrectly
  // labeled as text/plain instead.
  isJSON(mimeType, response) {
    let json, error;
    try {
      json = JSON.parse(response);
    } catch (err) {
      error = err;
    }

    if (/\bjson/.test(mimeType) || json) {
      // Extract the actual json substring in case this might be a "JSONP".
      // This regex basically parses a function call and captures the
      // function name and arguments in two separate groups.
      let jsonpRegex = /^\s*([\w$]+)\s*\(\s*([^]*)\s*\)\s*;?\s*$/;
      let [, jsonpCallback, jsonp] = response.match(jsonpRegex) || [];
      let result = {};

      // Make sure this is a valid JSON object first. If so, nicely display
      // the parsing results in a tree view.
      if (jsonpCallback && jsonp) {
        error = null;
        try {
          json = JSON.parse(jsonp);
        } catch (err) {
          error = err;
        }
      }

      // Valid JSON
      if (json) {
        result.json = json;
      }
      // Valid JSONP
      if (jsonpCallback) {
        result.jsonpCallback = jsonpCallback;
      }
      // Malformed JSON
      if (error) {
        result.error = "" + error;
      }

      return result;
    }

    return null;
  },

  render() {
    let { responseContent, url } = this.props.request;

    if (!responseContent || typeof responseContent.content.text !== "string") {
      return null;
    }

    let { encoding, mimeType, text } = responseContent.content;

    if (mimeType.includes("image/")) {
      let { width, height } = this.state.imageDimensions;

      return (
        div({ className: "panel-container response-image-box devtools-monospace" },
          img({
            className: "response-image",
            src: formDataURI(mimeType, encoding, text),
            onLoad: this.updateImageDimemsions,
          }),
          div({ className: "response-summary" },
            div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_NAME),
            div({ className: "tabpanel-summary-value" }, getUrlBaseName(url)),
          ),
          div({ className: "response-summary" },
            div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_DIMENSIONS),
            div({ className: "tabpanel-summary-value" }, `${width} × ${height}`),
          ),
          div({ className: "response-summary" },
            div({ className: "tabpanel-summary-label" }, RESPONSE_IMG_MIMETYPE),
            div({ className: "tabpanel-summary-value" }, mimeType),
          ),
        )
      );
    }

    // Display Properties View
    let { json, jsonpCallback, error } = this.isJSON(mimeType, text) || {};
    let object = {};
    let sectionName;

    if (json) {
      if (jsonpCallback) {
        sectionName = L10N.getFormatStr("jsonpScopeName", jsonpCallback);
      } else {
        sectionName = JSON_SCOPE_NAME;
      }
      object[sectionName] = json;
    } else {
      sectionName = RESPONSE_PAYLOAD;

      object[sectionName] = {
        EDITOR_CONFIG: {
          text,
          mode: mimeType.replace(/;.+/, ""),
        },
      };
    }

    return (
      div({ className: "panel-container" },
        error && div({ className: "response-error-header", title: error },
          error
        ),
        PropertiesView({
          object,
          filterPlaceHolder: JSON_FILTER_TEXT,
          sectionNames: [sectionName],
        }),
      )
    );
  }
});

module.exports = ResponsePanel;
