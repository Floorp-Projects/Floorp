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
  isJSON,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  Filters,
} = require("devtools/client/netmonitor/src/utils/filter-predicates");
const {
  FILTER_SEARCH_DELAY,
} = require("devtools/client/netmonitor/src/constants");

// Components
const PropertiesView = createFactory(
  require("devtools/client/netmonitor/src/components/request-details/PropertiesView")
);
const ImagePreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/ImagePreview")
);
const SourcePreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/SourcePreview")
);
const HtmlPreview = createFactory(
  require("devtools/client/netmonitor/src/components/previews/HtmlPreview")
);
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const SearchBox = createFactory(
  require("devtools/client/shared/components/SearchBox")
);

loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});

const { div } = dom;
const JSON_SCOPE_NAME = L10N.getStr("jsonScopeName");
const JSON_FILTER_TEXT = L10N.getStr("jsonFilterText");
const RESPONSE_PAYLOAD = L10N.getStr("responsePayload");
const RESPONSE_PREVIEW = L10N.getStr("responsePreview");
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
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      filterText: "",
      currentOpen: undefined,
    };
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
      (this.props.targetSearchResult !== nextProps.targetSearchResult &&
        nextProps.targetSearchResult !== null)
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

    let { json, error } = isJSON(response);

    if (/\bjson/.test(mimeType) || json) {
      // Extract the actual json substring in case this might be a "JSONP".
      // This regex basically parses a function call and captures the
      // function name and arguments in two separate groups.
      const jsonpRegex = /^\s*([\w$]+)\s*\(\s*([^]*)\s*\)\s*;?\s*$/;
      const [, jsonpCallback, jsonp] = response.match(jsonpRegex) || [];
      const result = {};

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
    const { request, targetSearchResult } = this.props;
    const { responseContent, url } = request;
    const { filterText } = this.state;

    if (
      !responseContent ||
      typeof responseContent.content.text !== "string" ||
      !responseContent.content.text
    ) {
      return div({ className: "empty-notice" }, RESPONSE_EMPTY_TEXT);
    }

    let { encoding, mimeType, text } = responseContent.content;

    if (mimeType.includes("image/")) {
      return ImagePreview({ encoding, mimeType, text, url });
    }

    // Decode response if it's coming from JSONView.
    if (mimeType.includes(JSON_VIEW_MIME_TYPE) && encoding === "base64") {
      text = decodeUnicodeBase64(text);
    }

    // Display Properties View
    const { json, jsonpCallback, error } =
      this.handleJSONResponse(mimeType, text) || {};

    const items = [];
    let sectionName;

    const onToggle = (open, item) => {
      this.setState({ currentOpen: open ? item : null });
    };

    if (json) {
      if (jsonpCallback) {
        sectionName = L10N.getFormatStr("jsonpScopeName", jsonpCallback);
      } else {
        sectionName = JSON_SCOPE_NAME;
      }

      items.push({
        component: PropertiesView,
        componentProps: {
          object: json,
          useQuotes: true,
          filterText,
          targetSearchResult,
          defaultSelectFirstNode: false,
          mode: MODE.LONG,
        },
        header: sectionName,
        id: "jsonpScopeName",
        opened: true,
        shouldOpen: item => {
          const { currentOpen } = this.state;
          if (typeof currentOpen == "undefined" && item.id === items[0].id) {
            // if this the first and panel just displayed, open this item
            // by default;
            return true;
          } else if (!currentOpen) {
            if (!targetSearchResult) {
              return false;
            }
            return true;
          }
          // Open the item is toggled open or there is a serch result to show
          if (item.id == currentOpen.id || targetSearchResult) {
            return true;
          }
          return false;
        },
        onToggle,
      });
    }

    // Display HTML
    if (Filters.html(this.props.request)) {
      items.push({
        component: HtmlPreview,
        componentProps: { responseContent },
        header: RESPONSE_PREVIEW,
        id: "responsePreview",
        opened: false,
        shouldOpen: item => {
          const { currentOpen } = this.state;
          if (typeof currentOpen == "undefined" && item.id === items[0].id) {
            // if this the first and panel just displayed, open this item
            // by default;
            if (targetSearchResult) {
              // collapse when we do a search
              return false;
            }
            return true;
          } else if (!currentOpen) {
            return false;
          }
          // close this if there is a search result since
          // it does not apply search
          if (targetSearchResult) {
            return false;
          }
          if (item.id == currentOpen.id) {
            return true;
          }
          return false;
        },
        onToggle,
      });
    }

    items.push({
      component: SourcePreview,
      componentProps: {
        text,
        mode: json ? "application/json" : mimeType.replace(/;.+/, ""),
        targetSearchResult,
        limit: Services.prefs.getIntPref(
          "devtools.netmonitor.response.ui.limit"
        ),
      },
      header: RESPONSE_PAYLOAD,
      id: "responsePayload",
      opened: !!targetSearchResult,
      shouldOpen: item => {
        const { currentOpen } = this.state;
        if (typeof currentOpen == "undefined" && item.id === items[0].id) {
          return true;
        } else if (!currentOpen) {
          if (targetSearchResult) {
            return true;
          }
          return false;
        }
        if (item.id == currentOpen.id || targetSearchResult) {
          return true;
        }
        return false;
      },
      onToggle,
    });

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
      Accordion({ items })
    );
  }
}

module.exports = ResponsePanel;
