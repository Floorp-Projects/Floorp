/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Services = require("Services");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  decodeUnicodeBase64,
  fetchNetworkUpdatePacket,
  parseJSON,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  getCORSErrorURL,
} = require("devtools/client/netmonitor/src/utils/doc-utils");
const {
  Filters,
} = require("devtools/client/netmonitor/src/utils/filter-predicates");
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");
const {
  BLOCKED_REASON_MESSAGES,
} = require("devtools/client/netmonitor/src/constants");

// Components
const PropertiesView = createFactory(
  require("devtools/client/netmonitor/src/components/request-details/PropertiesView")
);
const ImagePreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/ImagePreview")
);
const FontPreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/FontPreview")
);
const SourcePreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/SourcePreview")
);
const HtmlPreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/HtmlPreview")
);
let {
  NotificationBox,
  PriorityLevels,
} = require("devtools/client/shared/components/NotificationBox");
NotificationBox = createFactory(NotificationBox);
const MessagesView = createFactory(
  require("devtools/client/netmonitor/src/components/messages/MessagesView")
);
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/index").MODE;
});

const { div, input, label, span, h2 } = dom;
const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const JSON_FILTER_TEXT = L10N.getStr("jsonFilterText");
const RESPONSE_PAYLOAD = L10N.getStr("responsePayload");
const RAW_RESPONSE_PAYLOAD = L10N.getStr("netmonitor.response.raw");
const HTML_RESPONSE = L10N.getStr("netmonitor.response.html");
const RESPONSE_EMPTY_TEXT = L10N.getStr("responseEmptyText");
const RESPONSE_TRUNCATED = L10N.getStr("responseTruncated");

const JSON_VIEW_MIME_TYPE = "application/vnd.mozilla.json.view";

/**
 * Response panel component
 * Displays the GET parameters and POST data of a request
 */
class ResponsePanel extends Component {
  static get propTypes() {
    return {
      request: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      targetSearchResult: PropTypes.object,
      connector: PropTypes.object.isRequired,
      showMessagesView: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      filterText: "",
      rawResponsePayloadDisplayed: !!props.targetSearchResult,
    };

    this.toggleRawResponsePayload = this.toggleRawResponsePayload.bind(this);
    this.renderCORSBlockedReason = this.renderCORSBlockedReason.bind(this);
    this.renderRawResponsePayloadBtn = this.renderRawResponsePayloadBtn.bind(
      this
    );
    this.renderJsonHtmlAndSource = this.renderJsonHtmlAndSource.bind(this);
    this.handleJSONResponse = this.handleJSONResponse.bind(this);
  }

  componentDidMount() {
    const { request, connector } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "responseContent",
    ]);
  }

  componentWillReceiveProps(nextProps) {
    const { request, connector } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, request, [
      "responseContent",
    ]);

    // If the response contains XSSI stripped chars default to raw view
    const text = nextProps.request?.responseContent?.content?.text;
    const xssiStrippedChars = text && parseJSON(text)?.strippedChars;
    if (xssiStrippedChars && !this.state.rawResponsePayloadDisplayed) {
      this.toggleRawResponsePayload();
    }

    if (nextProps.targetSearchResult !== null) {
      this.setState({
        rawResponsePayloadDisplayed: !!nextProps.targetSearchResult,
      });
    }
  }

  /**
   * Update only if:
   * 1) The rendered object has changed
   * 2) The user selected another search result target.
   * 3) Internal state changes
   */
  shouldComponentUpdate(nextProps, nextState) {
    return (
      this.state !== nextState ||
      this.props.request !== nextProps.request ||
      nextProps.targetSearchResult !== null
    );
  }

  /**
   * Handle json, which we tentatively identify by checking the
   * MIME type for "json" after any word boundary. This works
   * for the standard "application/json", and also for custom
   * types like "x-bigcorp-json". Additionally, we also
   * directly parse the response text content to verify whether
   * it's json or not, to handle responses incorrectly labeled
   * as text/plain instead.
   */
  handleJSONResponse(mimeType, response) {
    const limit = Services.prefs.getIntPref(
      "devtools.netmonitor.responseBodyLimit"
    );
    const { request } = this.props;

    // Check if the response has been truncated, in which case no parse should
    // be attempted.
    if (limit > 0 && limit <= request.responseContent.content.size) {
      const result = {};
      result.error = RESPONSE_TRUNCATED;
      return result;
    }

    const { json, error, jsonpCallback, strippedChars } = parseJSON(response);

    if (/\bjson/.test(mimeType) || json) {
      const result = {};
      // Make sure this is a valid JSON object first. If so, nicely display
      // the parsing results in a tree view.

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
      // XSSI protection sequence
      if (strippedChars) {
        result.strippedChars = strippedChars;
      }

      return result;
    }

    return null;
  }

  renderCORSBlockedReason(blockedReason) {
    // ensure that the blocked reason is in the CORS range
    if (
      typeof blockedReason != "number" ||
      blockedReason < 1000 ||
      blockedReason > 1015
    ) {
      return null;
    }

    const blockedMessage = BLOCKED_REASON_MESSAGES[blockedReason];
    const messageText = L10N.getFormatStr(
      "netmonitor.headers.blockedByCORS",
      blockedMessage
    );

    const learnMoreTooltip = L10N.getStr(
      "netmonitor.headers.blockedByCORSTooltip"
    );

    // Create a notifications map with the CORS error notification
    const notifications = new Map();
    notifications.set("CORS-error", {
      label: messageText,
      value: "CORS-error",
      image: "",
      priority: PriorityLevels.PRIORITY_INFO_HIGH,
      type: "info",
      eventCallback: e => {},
      buttons: [
        {
          mdnUrl: getCORSErrorURL(blockedReason),
          label: learnMoreTooltip,
        },
      ],
    });

    return NotificationBox({
      notifications,
      displayBorderTop: false,
      displayBorderBottom: true,
      displayCloseButton: false,
    });
  }

  toggleRawResponsePayload() {
    this.setState({
      rawResponsePayloadDisplayed: !this.state.rawResponsePayloadDisplayed,
    });
  }

  /**
   * Pick correct component, componentprops, and other needed data to render
   * the given response
   *
   * @returns {Object} shape:
   *  {component}: React component used to render response
   *  {Object} componetProps: Props passed to component
   *  {Error} error: JSON parsing error
   *  {Object} json: parsed JSON payload
   *  {bool} hasFormattedDisplay: whether the given payload has a formatted
   *         display or if it should be rendered raw
   *  {string} responsePayloadLabel: describes type in response panel
   *  {component} xssiStrippedCharsInfoBox: React component to notifiy users
   *              that XSSI characters were stripped from the response
   */
  renderJsonHtmlAndSource() {
    const { request, targetSearchResult } = this.props;
    const { responseContent } = request;
    let { encoding, mimeType, text } = responseContent.content;
    const { filterText, rawResponsePayloadDisplayed } = this.state;

    // Decode response if it's coming from JSONView.
    if (mimeType?.includes(JSON_VIEW_MIME_TYPE) && encoding === "base64") {
      text = decodeUnicodeBase64(text);
    }
    const { json, jsonpCallback, error, strippedChars } =
      this.handleJSONResponse(mimeType, text) || {};

    let component;
    let componentProps;
    let xssiStrippedCharsInfoBox;
    let responsePayloadLabel = RESPONSE_PAYLOAD;
    let hasFormattedDisplay = false;

    if (json) {
      if (jsonpCallback) {
        responsePayloadLabel = L10N.getFormatStr(
          "jsonpScopeName",
          jsonpCallback
        );
      } else {
        responsePayloadLabel = JSON_SCOPE_NAME;
      }

      // If raw response payload is not displayed render xssi info box if
      // there are stripped chars
      if (!rawResponsePayloadDisplayed) {
        xssiStrippedCharsInfoBox = this.renderXssiStrippedCharsInfoBox(
          strippedChars
        );
      } else {
        xssiStrippedCharsInfoBox = null;
      }

      component = PropertiesView;
      componentProps = {
        object: json,
        useQuotes: true,
        filterText,
        targetSearchResult,
        defaultSelectFirstNode: false,
        mode: MODE.LONG,
        useBaseTreeViewExpand: true,
      };
      hasFormattedDisplay = true;
    } else if (Filters.html(this.props.request)) {
      // Display HTML
      responsePayloadLabel = HTML_RESPONSE;
      component = HtmlPreview;
      componentProps = { responseContent };
      hasFormattedDisplay = true;
    }
    if (!hasFormattedDisplay || rawResponsePayloadDisplayed) {
      component = SourcePreview;
      componentProps = {
        text,
        mode: json ? "application/json" : mimeType.replace(/;.+/, ""),
        targetSearchResult,
      };
    }
    return {
      component,
      componentProps,
      error,
      hasFormattedDisplay,
      json,
      responsePayloadLabel,
      xssiStrippedCharsInfoBox,
    };
  }

  renderRawResponsePayloadBtn(key, checked, onChange) {
    return [
      label(
        {
          key: `${key}RawResponsePayloadBtn`,
          className: "raw-data-toggle",
          htmlFor: `raw-${key}-checkbox`,
          onClick: event => {
            // stop the header click event
            event.stopPropagation();
          },
        },
        span({ className: "raw-data-toggle-label" }, RAW_RESPONSE_PAYLOAD),
        span(
          { className: "raw-data-toggle-input" },
          input({
            id: `raw-${key}-checkbox`,
            checked,
            className: "devtools-checkbox-toggle",
            onChange,
            type: "checkbox",
          })
        )
      ),
    ];
  }

  renderResponsePayload(component, componentProps) {
    return component(componentProps);
  }

  /**
   * This function takes a string of the XSSI protection characters
   * removed from a JSON payload and produces a notification component
   * letting the user know that they were removed
   *
   * @param {string} strippedChars: string of XSSI protection characters
   *                                removed from JSON payload
   * @returns {component} NotificationBox component
   */
  renderXssiStrippedCharsInfoBox(strippedChars) {
    if (!strippedChars || this.state.rawRequestPayloadDisplayed) {
      return null;
    }
    const message = L10N.getFormatStr("jsonXssiStripped", strippedChars);

    const notifications = new Map();
    notifications.set("xssi-string-removed-info-box", {
      label: message,
      value: "xssi-string-removed-info-box",
      image: "",
      priority: PriorityLevels.PRIORITY_INFO_MEDIUM,
      type: "info",
      eventCallback: e => {},
      buttons: [],
    });

    return NotificationBox({
      notifications,
      displayBorderTop: false,
      displayBorderBottom: true,
      displayCloseButton: false,
    });
  }

  render() {
    const { connector, showMessagesView, request } = this.props;
    const { blockedReason, responseContent, url } = request;
    const { filterText, rawResponsePayloadDisplayed } = this.state;

    // Display CORS blocked Reason info box
    const CORSBlockedReasonDetails = this.renderCORSBlockedReason(
      blockedReason
    );

    if (showMessagesView) {
      return MessagesView({ connector });
    }

    if (
      !responseContent ||
      typeof responseContent.content.text !== "string" ||
      !responseContent.content.text
    ) {
      return div(
        { className: "panel-container" },
        CORSBlockedReasonDetails,
        div({ className: "empty-notice" }, RESPONSE_EMPTY_TEXT)
      );
    }

    const { encoding, mimeType, text } = responseContent.content;

    if (Filters.images({ mimeType })) {
      return ImagePreview({ encoding, mimeType, text, url });
    }

    if (Filters.fonts({ url, mimeType })) {
      return FontPreview({ connector, mimeType, url });
    }

    // Get Data needed for formatted display
    const {
      component,
      componentProps,
      error,
      hasFormattedDisplay,
      json,
      responsePayloadLabel,
      xssiStrippedCharsInfoBox,
    } = this.renderJsonHtmlAndSource();

    const classList = ["panel-container"];
    if (Filters.html(this.props.request)) {
      classList.push("contains-html-preview");
    }

    return div(
      { className: classList.join(" ") },
      error && div({ className: "response-error-header", title: error }, error),
      json &&
        div(
          { className: "devtools-toolbar devtools-input-toolbar" },
          SearchBox({
            delay: FILTER_SEARCH_DELAY,
            type: "filter",
            onChange: filter => this.setState({ filterText: filter }),
            placeholder: JSON_FILTER_TEXT,
            value: filterText,
          })
        ),
      div({ tabIndex: "0" }, CORSBlockedReasonDetails),
      h2({ className: "data-header", role: "heading" }, [
        span(
          {
            key: "data-label",
            className: "data-label",
          },
          responsePayloadLabel
        ),
        hasFormattedDisplay &&
          this.renderRawResponsePayloadBtn(
            "response",
            rawResponsePayloadDisplayed,
            this.toggleRawResponsePayload
          ),
      ]),
      xssiStrippedCharsInfoBox,
      this.renderResponsePayload(component, componentProps)
    );
  }
}

module.exports = ResponsePanel;
