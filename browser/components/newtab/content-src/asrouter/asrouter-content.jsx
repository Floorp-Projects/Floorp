/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
  ASRouterActions as ra,
} from "common/Actions.jsm";
import { OUTGOING_MESSAGE_NAME as AS_GENERAL_OUTGOING_MESSAGE_NAME } from "content-src/lib/init-store";
import { generateBundles } from "./rich-text-strings";
import { ImpressionsWrapper } from "./components/ImpressionsWrapper/ImpressionsWrapper";
import { LocalizationProvider } from "fluent-react";
import { NEWTAB_DARK_THEME } from "content-src/lib/constants";
import React from "react";
import ReactDOM from "react-dom";
import { SnippetsTemplates } from "./templates/template-manifest";
import { FirstRun } from "./templates/FirstRun/FirstRun";

const INCOMING_MESSAGE_NAME = "ASRouter:parent-to-child";
const OUTGOING_MESSAGE_NAME = "ASRouter:child-to-parent";
const TEMPLATES_ABOVE_PAGE = [
  "trailhead",
  "full_page_interrupt",
  "return_to_amo_overlay",
  "extended_triplets",
];
const FIRST_RUN_TEMPLATES = TEMPLATES_ABOVE_PAGE;
const TEMPLATES_BELOW_SEARCH = ["simple_below_search_snippet"];

export const ASRouterUtils = {
  addListener(listener) {
    if (global.RPMAddMessageListener) {
      global.RPMAddMessageListener(INCOMING_MESSAGE_NAME, listener);
    }
  },
  removeListener(listener) {
    if (global.RPMRemoveMessageListener) {
      global.RPMRemoveMessageListener(INCOMING_MESSAGE_NAME, listener);
    }
  },
  sendMessage(action) {
    if (global.RPMSendAsyncMessage) {
      global.RPMSendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
    }
  },
  blockById(id, options) {
    ASRouterUtils.sendMessage({
      type: "BLOCK_MESSAGE_BY_ID",
      data: { id, ...options },
    });
  },
  dismissById(id) {
    ASRouterUtils.sendMessage({ type: "DISMISS_MESSAGE_BY_ID", data: { id } });
  },
  executeAction(button_action) {
    ASRouterUtils.sendMessage({
      type: "USER_ACTION",
      data: button_action,
    });
  },
  unblockById(id) {
    ASRouterUtils.sendMessage({ type: "UNBLOCK_MESSAGE_BY_ID", data: { id } });
  },
  blockBundle(bundle) {
    ASRouterUtils.sendMessage({ type: "BLOCK_BUNDLE", data: { bundle } });
  },
  unblockBundle(bundle) {
    ASRouterUtils.sendMessage({ type: "UNBLOCK_BUNDLE", data: { bundle } });
  },
  overrideMessage(id) {
    ASRouterUtils.sendMessage({ type: "OVERRIDE_MESSAGE", data: { id } });
  },
  sendTelemetry(ping) {
    if (global.RPMSendAsyncMessage) {
      const payload = ac.ASRouterUserEvent(ping);
      global.RPMSendAsyncMessage(AS_GENERAL_OUTGOING_MESSAGE_NAME, payload);
    }
  },
  getPreviewEndpoint() {
    if (global.location && global.location.href.includes("endpoint")) {
      const params = new URLSearchParams(
        global.location.href.slice(global.location.href.indexOf("endpoint"))
      );
      try {
        const endpoint = new URL(params.get("endpoint"));
        return {
          url: endpoint.href,
          snippetId: params.get("snippetId"),
          theme: this.getPreviewTheme(),
          dir: this.getPreviewDir(),
        };
      } catch (e) {}
    }

    return null;
  },
  getPreviewTheme() {
    return new URLSearchParams(
      global.location.href.slice(global.location.href.indexOf("theme"))
    ).get("theme");
  },
  getPreviewDir() {
    return new URLSearchParams(
      global.location.href.slice(global.location.href.indexOf("dir"))
    ).get("dir");
  },
};

// Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />
function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return (
    nextProps.message.id &&
    (!prevProps.message || prevProps.message.id !== nextProps.message.id)
  );
}

export class ASRouterUISurface extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.sendClick = this.sendClick.bind(this);
    this.sendImpression = this.sendImpression.bind(this);
    this.sendUserActionTelemetry = this.sendUserActionTelemetry.bind(this);
    this.onUserAction = this.onUserAction.bind(this);
    this.fetchFlowParams = this.fetchFlowParams.bind(this);

    this.state = { message: {}, interruptCleared: false };
    if (props.document) {
      this.headerPortal = props.document.getElementById(
        "header-asrouter-container"
      );
      this.footerPortal = props.document.getElementById(
        "footer-asrouter-container"
      );
    }
  }

  async fetchFlowParams(params = {}) {
    let result = {};
    const { fxaEndpoint, dispatch } = this.props;
    if (!fxaEndpoint) {
      const err =
        "Tried to fetch flow params before fxaEndpoint pref was ready";
      console.error(err); // eslint-disable-line no-console
    }

    try {
      const urlObj = new URL(fxaEndpoint);
      urlObj.pathname = "metrics-flow";
      Object.keys(params).forEach(key => {
        urlObj.searchParams.append(key, params[key]);
      });
      const response = await fetch(urlObj.toString(), { credentials: "omit" });
      if (response.status === 200) {
        const { deviceId, flowId, flowBeginTime } = await response.json();
        result = { deviceId, flowId, flowBeginTime };
      } else {
        console.error("Non-200 response", response); // eslint-disable-line no-console
        dispatch(
          ac.OnlyToMain({
            type: at.TELEMETRY_UNDESIRED_EVENT,
            data: {
              event: "FXA_METRICS_FETCH_ERROR",
              value: response.status,
            },
          })
        );
      }
    } catch (error) {
      console.error(error); // eslint-disable-line no-console
      dispatch(
        ac.OnlyToMain({
          type: at.TELEMETRY_UNDESIRED_EVENT,
          data: { event: "FXA_METRICS_ERROR" },
        })
      );
    }
    return result;
  }

  sendUserActionTelemetry(extraProps = {}) {
    const { message } = this.state;
    const eventType = `${message.provider}_user_event`;
    ASRouterUtils.sendTelemetry({
      message_id: message.id,
      source: extraProps.id,
      action: eventType,
      ...extraProps,
    });
  }

  sendImpression(extraProps) {
    if (this.state.message.provider === "preview") {
      return;
    }

    ASRouterUtils.sendMessage({ type: "IMPRESSION", data: this.state.message });
    this.sendUserActionTelemetry({ event: "IMPRESSION", ...extraProps });
  }

  // If link has a `metric` data attribute send it as part of the `event_context`
  // telemetry field which can have arbitrary values.
  // Used for router messages with links as part of the content.
  sendClick(event) {
    const metric = {
      event_context: event.target.dataset.metric,
      // Used for the `source` of the event. Needed to differentiate
      // from other snippet or onboarding events that may occur.
      id: "NEWTAB_FOOTER_BAR_CONTENT",
    };
    const action = {
      type: event.target.dataset.action,
      data: { args: event.target.dataset.args },
    };
    if (action.type) {
      ASRouterUtils.executeAction(action);
    }
    if (
      !this.state.message.content.do_not_autoblock &&
      !event.target.dataset.do_not_autoblock
    ) {
      ASRouterUtils.blockById(this.state.message.id);
    }
    if (this.state.message.provider !== "preview") {
      this.sendUserActionTelemetry({ event: "CLICK_BUTTON", ...metric });
    }
  }

  onBlockById(id) {
    return options => ASRouterUtils.blockById(id, options);
  }

  onDismissById(id) {
    return () => ASRouterUtils.dismissById(id);
  }

  clearMessage(id) {
    // Request new set of dynamic triplet cards when click on a card CTA clear
    // message and 'id' matches one of the cards in message bundle
    if (
      this.state.message &&
      this.state.message.bundle &&
      this.state.message.bundle.find(card => card.id === id)
    ) {
      this.requestMessage();
    }

    if (id === this.state.message.id) {
      this.setState({ message: {} });
      // Remove any styles related to the RTAMO message
      document.body.classList.remove("welcome", "hide-main", "amo");
    }
  }

  onMessageFromParent({ data: action }) {
    switch (action.type) {
      case "SET_MESSAGE":
        this.setState({ message: action.data });
        break;
      case "CLEAR_INTERRUPT":
        this.setState({ interruptCleared: true });
        break;
      case "CLEAR_MESSAGE":
        this.clearMessage(action.data.id);
        break;
      case "CLEAR_PROVIDER":
        if (action.data.id === this.state.message.provider) {
          this.setState({ message: {} });
        }
        break;
      case "CLEAR_ALL":
        this.setState({ message: {} });
        break;
      case "AS_ROUTER_TARGETING_UPDATE":
        action.data.forEach(id => this.clearMessage(id));
        break;
    }
  }

  requestMessage(endpoint) {
    // If we are loading about:welcome we want to trigger the onboarding messages
    if (
      this.props.document &&
      this.props.document.location.href === "about:welcome"
    ) {
      ASRouterUtils.sendMessage({
        type: "TRIGGER",
        data: { trigger: { id: "firstRun" } },
      });
    } else {
      ASRouterUtils.sendMessage({
        type: "NEWTAB_MESSAGE_REQUEST",
        data: { endpoint },
      });
    }
  }

  componentWillMount() {
    const endpoint = ASRouterUtils.getPreviewEndpoint();
    if (endpoint && endpoint.theme === "dark") {
      global.window.dispatchEvent(
        new CustomEvent("LightweightTheme:Set", {
          detail: { data: NEWTAB_DARK_THEME },
        })
      );
    }
    if (endpoint && endpoint.dir === "rtl") {
      //Set `dir = rtl` on the HTML
      this.props.document.dir = "rtl";
    }
    ASRouterUtils.addListener(this.onMessageFromParent);
    this.requestMessage(endpoint);
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  async getMonitorUrl({ url, flowRequestParams = {} }) {
    const flowValues = await this.fetchFlowParams(flowRequestParams);

    // Note that flowParams are actually added dynamically on the page
    const urlObj = new URL(url);
    ["deviceId", "flowId", "flowBeginTime"].forEach(key => {
      if (key in flowValues) {
        urlObj.searchParams.append(key, flowValues[key]);
      }
    });

    return urlObj.toString();
  }

  async onUserAction(action) {
    switch (action.type) {
      // This needs to be handled locally because its
      case ra.ENABLE_FIREFOX_MONITOR:
        const url = await this.getMonitorUrl(action.data.args);
        ASRouterUtils.executeAction({ type: ra.OPEN_URL, data: { args: url } });
        break;
      default:
        ASRouterUtils.executeAction(action);
    }
  }

  renderSnippets() {
    const { message } = this.state;
    if (!SnippetsTemplates[message.template]) {
      return null;
    }
    const SnippetComponent = SnippetsTemplates[message.template];
    const { content } = this.state.message;

    return (
      <ImpressionsWrapper
        id="NEWTAB_FOOTER_BAR"
        message={this.state.message}
        sendImpression={this.sendImpression}
        shouldSendImpressionOnUpdate={shouldSendImpressionOnUpdate}
        // This helps with testing
        document={this.props.document}
      >
        <LocalizationProvider bundles={generateBundles(content)}>
          <SnippetComponent
            {...this.state.message}
            UISurface="NEWTAB_FOOTER_BAR"
            onBlock={this.onBlockById(this.state.message.id)}
            onDismiss={this.onDismissById(this.state.message.id)}
            onAction={this.onUserAction}
            sendClick={this.sendClick}
            sendUserActionTelemetry={this.sendUserActionTelemetry}
          />
        </LocalizationProvider>
      </ImpressionsWrapper>
    );
  }

  renderPreviewBanner() {
    if (this.state.message.provider !== "preview") {
      return null;
    }

    return (
      <div className="snippets-preview-banner">
        <span className="icon icon-small-spacer icon-info" />
        <span>Preview Purposes Only</span>
      </div>
    );
  }

  renderFirstRun() {
    const { message } = this.state;
    if (FIRST_RUN_TEMPLATES.includes(message.template)) {
      return (
        <ImpressionsWrapper
          id="FIRST_RUN"
          message={this.state.message}
          sendImpression={this.sendImpression}
          shouldSendImpressionOnUpdate={shouldSendImpressionOnUpdate}
          // This helps with testing
          document={this.props.document}
        >
          <FirstRun
            document={this.props.document}
            interruptCleared={this.state.interruptCleared}
            message={message}
            sendUserActionTelemetry={this.sendUserActionTelemetry}
            executeAction={ASRouterUtils.executeAction}
            dispatch={this.props.dispatch}
            onBlockById={ASRouterUtils.blockById}
            onDismiss={this.onDismissById(this.state.message.id)}
            fxaEndpoint={this.props.fxaEndpoint}
            appUpdateChannel={this.props.appUpdateChannel}
            fetchFlowParams={this.fetchFlowParams}
          />
        </ImpressionsWrapper>
      );
    }
    return null;
  }

  render() {
    const { message } = this.state;
    if (!message.id) {
      return null;
    }
    const shouldRenderBelowSearch = TEMPLATES_BELOW_SEARCH.includes(
      message.template
    );
    const shouldRenderInHeader = TEMPLATES_ABOVE_PAGE.includes(
      message.template
    );

    return shouldRenderBelowSearch ? (
      // Render special below search snippets in place;
      <div className="below-search-snippet-wrapper">
        {this.renderSnippets()}
      </div>
    ) : (
      // For onboarding, regular snippets etc. we should render
      // everything in our footer container.
      ReactDOM.createPortal(
        <>
          {this.renderPreviewBanner()}
          {this.renderFirstRun()}
          {this.renderSnippets()}
        </>,
        shouldRenderInHeader ? this.headerPortal : this.footerPortal
      )
    );
  }
}

ASRouterUISurface.defaultProps = { document: global.document };
