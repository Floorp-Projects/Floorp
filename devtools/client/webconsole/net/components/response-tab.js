/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory, DOM, PropTypes } =
  require("devtools/client/shared/vendor/react");

// Reps
const TreeView = createFactory(require("devtools/client/shared/components/tree/TreeView"));
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;

// Network
const SizeLimit = createFactory(require("./size-limit"));
const NetInfoGroupList = createFactory(require("./net-info-group-list"));
const Spinner = createFactory(require("./spinner"));
const Json = require("../utils/json");
const NetUtils = require("../utils/net");

/**
 * This template represents 'Response' tab displayed when the user
 * expands network log in the Console panel. It's responsible for
 * rendering HTTP response body.
 *
 * In case of supported response mime-type (e.g. application/json,
 * text/xml, etc.), the response is parsed using appropriate parser
 * and rendered accordingly.
 */
class ResponseTab extends Component {
  static get propTypes() {
    return {
      data: PropTypes.shape({
        request: PropTypes.object.isRequired,
        response: PropTypes.object.isRequired
      }),
      actions: PropTypes.object.isRequired
    };
  }

  constructor(props) {
    super(props);
    this.isJson = this.isJson.bind(this);
    this.parseJson = this.parseJson.bind(this);
    this.isImage = this.isImage.bind(this);
    this.isXml = this.isXml.bind(this);
    this.parseXml = this.parseXml.bind(this);
    this.renderJson = this.renderJson.bind(this);
    this.renderImage = this.renderImage.bind(this);
    this.renderXml = this.renderXml.bind(this);
    this.renderFormattedResponse = this.renderFormattedResponse.bind(this);
    this.renderRawResponse = this.renderRawResponse.bind(this);
  }

  // Response Types

  isJson(content) {
    if (isLongString(content.text)) {
      return false;
    }

    return Json.isJSON(content.mimeType, content.text);
  }

  parseJson(file) {
    let content = file.response.content;
    if (isLongString(content.text)) {
      return null;
    }

    let jsonString = new String(content.text);
    return Json.parseJSONString(jsonString);
  }

  isImage(content) {
    if (isLongString(content.text)) {
      return false;
    }

    return NetUtils.isImage(content.mimeType);
  }

  isXml(content) {
    if (isLongString(content.text)) {
      return false;
    }

    return NetUtils.isHTML(content.mimeType);
  }

  parseXml(file) {
    let content = file.response.content;
    if (isLongString(content.text)) {
      return null;
    }

    return NetUtils.parseXml(content);
  }

  // Rendering

  renderJson(file) {
    let content = file.response.content;
    if (!this.isJson(content)) {
      return null;
    }

    let json = this.parseJson(file);
    if (!json) {
      return null;
    }

    return {
      key: "json",
      content: TreeView({
        columns: [{id: "value"}],
        object: json,
        mode: MODE.TINY,
        renderValue: props => Rep(Object.assign({}, props, {
          cropLimit: 50,
        })),
      }),
      name: Locale.$STR("jsonScopeName")
    };
  }

  renderImage(file) {
    let content = file.response.content;
    if (!this.isImage(content)) {
      return null;
    }

    let dataUri = "data:" + content.mimeType + ";base64," + content.text;
    return {
      key: "image",
      content: DOM.img({src: dataUri}),
      name: Locale.$STR("netRequest.image")
    };
  }

  renderXml(file) {
    let content = file.response.content;
    if (!this.isXml(content)) {
      return null;
    }

    let doc = this.parseXml(file);
    if (!doc) {
      return null;
    }

    // Proper component for rendering XML should be used (see bug 1247392)
    return null;
  }

  /**
   * If full response text is available, let's try to parse and
   * present nicely according to the underlying format.
   */
  renderFormattedResponse(file) {
    let content = file.response.content;
    if (typeof content.text == "object") {
      return null;
    }

    let group = this.renderJson(file);
    if (group) {
      return group;
    }

    group = this.renderImage(file);
    if (group) {
      return group;
    }

    group = this.renderXml(file);
    if (group) {
      return group;
    }
  }

  renderRawResponse(file) {
    let group;
    let content = file.response.content;

    // The response might reached the limit, so check if we are
    // dealing with a long string.
    if (typeof content.text == "object") {
      group = {
        key: "raw-longstring",
        name: Locale.$STR("netRequest.rawData"),
        content: DOM.div({className: "netInfoResponseContent"},
          content.text.initial,
          SizeLimit({
            actions: this.props.actions,
            data: content,
            message: Locale.$STR("netRequest.sizeLimitMessage"),
            link: Locale.$STR("netRequest.sizeLimitMessageLink")
          })
        )
      };
    } else {
      group = {
        key: "raw",
        name: Locale.$STR("netRequest.rawData"),
        content: DOM.div({className: "netInfoResponseContent"},
          content.text
        )
      };
    }

    return group;
  }

  componentDidMount() {
    let { actions, data: file } = this.props;
    let content = file.response.content;

    if (!content || typeof (content.text) == "undefined") {
      // TODO: use async action objects as soon as Redux is in place
      actions.requestData("responseContent");
    }
  }

  /**
   * The response panel displays two groups:
   *
   * 1) Formatted response (in case of supported format, e.g. JSON, XML, etc.)
   * 2) Raw response data (always displayed if not discarded)
   */
  render() {
    let { actions, data: file } = this.props;

    // If response bodies are discarded (not collected) let's just
    // display a info message indicating what to do to collect even
    // response bodies.
    if (file.discardResponseBody) {
      return DOM.span({className: "netInfoBodiesDiscarded"},
        Locale.$STR("netRequest.responseBodyDiscarded")
      );
    }

    // Request for the response content is done only if the response
    // is not fetched yet - i.e. the `content.text` is undefined.
    // Empty content.text` can also be a valid response either
    // empty or not available yet.
    let content = file.response.content;
    if (!content || typeof (content.text) == "undefined") {
      return (
        Spinner()
      );
    }

    // Render response body data. The right representation of the data
    // is picked according to the content type.
    let groups = [];
    groups.push(this.renderFormattedResponse(file));
    groups.push(this.renderRawResponse(file));

    // Filter out empty groups.
    groups = groups.filter(group => group);

    // The raw response is collapsed by default if a nice formatted
    // version is available.
    if (groups.length > 1) {
      groups[1].open = false;
    }

    return (
      DOM.div({className: "responseTabBox"},
        DOM.div({className: "panelContent"},
          NetInfoGroupList({
            groups: groups
          })
        )
      )
    );
  }
}

// Helpers

function isLongString(text) {
  return typeof text == "object";
}

// Exports from this module
module.exports = ResponseTab;
