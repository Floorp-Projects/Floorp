/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const {
  decodeUnicodeBase64,
  formDataURI,
  getUrlBaseName,
} = require("../utils/request-utils");
const { Filters } = require("../utils/filter-predicates");

// Components
const PropertiesView = createFactory(require("./PropertiesView"));

const { div, img } = dom;
const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const JSON_FILTER_TEXT = L10N.getStr("jsonFilterText");
const RESPONSE_IMG_NAME = L10N.getStr("netmonitor.response.name");
const RESPONSE_IMG_DIMENSIONS = L10N.getStr("netmonitor.response.dimensions");
const RESPONSE_IMG_MIMETYPE = L10N.getStr("netmonitor.response.mime");
const RESPONSE_PAYLOAD = L10N.getStr("responsePayload");
const RESPONSE_PREVIEW = L10N.getStr("responsePreview");

const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";

/*
 * Response panel component
 * Displays the GET parameters and POST data of a request
 */
class ResponsePanel extends Component {
  static get propTypes() {
    return {
      request: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      connector: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      imageDimensions: {
        width: 0,
        height: 0,
      },
    };

    this.updateImageDimemsions = this.updateImageDimemsions.bind(this);
    this.isJSON = this.isJSON.bind(this);
  }

  componentDidMount() {
    this.maybeFetchResponseContent(this.props);
  }

  componentWillReceiveProps(nextProps) {
    this.maybeFetchResponseContent(nextProps);
  }

  /**
   * When switching to another request, lazily fetch response content
   * from the backend. The Response Panel will first be empty and then
   * display the content.
   */
  maybeFetchResponseContent(props) {
    if (props.request.responseContentAvailable &&
        (!props.request.responseContent ||
         !props.request.responseContent.content)) {
      // This method will set `props.request.responseContent.content`
      // asynchronously and force another render.
      props.connector.requestData(props.request.id, "responseContent");
    }
  }

  updateImageDimemsions({ target }) {
    this.setState({
      imageDimensions: {
        width: target.naturalWidth,
        height: target.naturalHeight,
      },
    });
  }

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
      try {
        json = JSON.parse(atob(response));
      } catch (err64) {
        error = err;
      }
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
  }

  render() {
    let { openLink, request } = this.props;
    let { responseContent, url } = request;

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

    // Decode response if it's coming from JSONView.
    if (mimeType.includes(JSON_VIEW_MIME_TYPE) && encoding === "base64") {
      text = decodeUnicodeBase64(text);
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
    }

    // Display HTML under Properties View
    if (Filters.html(this.props.request)) {
      object[RESPONSE_PREVIEW] = {
        HTML_PREVIEW: { responseContent }
      };
    }

    // Others like text/html, text/plain, application/javascript
    object[RESPONSE_PAYLOAD] = {
      EDITOR_CONFIG: {
        text,
        mode: json ? "application/json" : mimeType.replace(/;.+/, ""),
      },
    };

    let classList = ["panel-container"];
    if (Filters.html(this.props.request)) {
      classList.push("contains-html-preview");
    }

    return (
      div({ className: classList.join(" ") },
        error && div({ className: "response-error-header", title: error },
          error
        ),
        PropertiesView({
          object,
          filterPlaceHolder: JSON_FILTER_TEXT,
          sectionNames: Object.keys(object),
          openLink,
        }),
      )
    );
  }
}

module.exports = ResponsePanel;
