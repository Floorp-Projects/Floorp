import {addLocaleData, IntlProvider} from "react-intl";
import {actionCreators as ac} from "common/Actions.jsm";
import {OUTGOING_MESSAGE_NAME as AS_GENERAL_OUTGOING_MESSAGE_NAME} from "content-src/lib/init-store";
import {generateMessages} from "./rich-text-strings";
import {ImpressionsWrapper} from "./components/ImpressionsWrapper/ImpressionsWrapper";
import {LocalizationProvider} from "fluent-react";
import {OnboardingMessage} from "./templates/OnboardingMessage/OnboardingMessage";
import React from "react";
import ReactDOM from "react-dom";
import {ReturnToAMO} from "./templates/ReturnToAMO/ReturnToAMO";
import {SnippetsTemplates} from "./templates/template-manifest";
import {StartupOverlay} from "./templates/StartupOverlay/StartupOverlay";

const INCOMING_MESSAGE_NAME = "ASRouter:parent-to-child";
const OUTGOING_MESSAGE_NAME = "ASRouter:child-to-parent";
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
    ASRouterUtils.sendMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id, ...options}});
  },
  dismissById(id) {
    ASRouterUtils.sendMessage({type: "DISMISS_MESSAGE_BY_ID", data: {id}});
  },
  dismissBundle(bundle) {
    ASRouterUtils.sendMessage({type: "DISMISS_BUNDLE", data: {bundle}});
  },
  executeAction(button_action) {
    ASRouterUtils.sendMessage({
      type: "USER_ACTION",
      data: button_action,
    });
  },
  unblockById(id) {
    ASRouterUtils.sendMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id}});
  },
  unblockBundle(bundle) {
    ASRouterUtils.sendMessage({type: "UNBLOCK_BUNDLE", data: {bundle}});
  },
  overrideMessage(id) {
    ASRouterUtils.sendMessage({type: "OVERRIDE_MESSAGE", data: {id}});
  },
  sendTelemetry(ping) {
    if (global.RPMSendAsyncMessage) {
      const payload = ac.ASRouterUserEvent(ping);
      global.RPMSendAsyncMessage(AS_GENERAL_OUTGOING_MESSAGE_NAME, payload);
    }
  },
  getPreviewEndpoint() {
    if (global.location && global.location.href.includes("endpoint")) {
      const params = new URLSearchParams(global.location.href.slice(global.location.href.indexOf("endpoint")));
      try {
        const endpoint = new URL(params.get("endpoint"));
        return {
          url: endpoint.href,
          snippetId: params.get("snippetId"),
        };
      } catch (e) {}
    }

    return null;
  },
};

// Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />
function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return (nextProps.message.id && (!prevProps.message || prevProps.message.id !== nextProps.message.id));
}

export class ASRouterUISurface extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.sendClick = this.sendClick.bind(this);
    this.sendImpression = this.sendImpression.bind(this);
    this.sendUserActionTelemetry = this.sendUserActionTelemetry.bind(this);
    this.state = {message: {}, bundle: {}};
    if (props.document) {
      this.portalContainer = props.document.getElementById("footer-snippets-container");
    }
  }

  sendUserActionTelemetry(extraProps = {}) {
    const {message, bundle} = this.state;
    if (!message && !extraProps.message_id) {
      throw new Error(`You must provide a message_id for bundled messages`);
    }
    const eventType = `${message.provider || bundle.provider}_user_event`;
    ASRouterUtils.sendTelemetry({
      message_id: message.id || extraProps.message_id,
      source: extraProps.id,
      action: eventType,
      ...extraProps,
    });
  }

  sendImpression(extraProps) {
    if (this.state.message.provider === "preview") {
      return;
    }

    ASRouterUtils.sendMessage({type: "IMPRESSION", data: this.state.message});
    this.sendUserActionTelemetry({event: "IMPRESSION", ...extraProps});
  }

  // If link has a `metric` data attribute send it as part of the `value`
  // telemetry field which can have arbitrary values.
  // Used for router messages with links as part of the content.
  sendClick(event) {
    const metric = {
      value: event.target.dataset.metric,
      // Used for the `source` of the event. Needed to differentiate
      // from other snippet or onboarding events that may occur.
      id: "NEWTAB_FOOTER_BAR_CONTENT",
    };
    const action = {
      type: event.target.dataset.action,
      data: {args: event.target.dataset.args},
    };
    if (action.type) {
      ASRouterUtils.executeAction(action);
    }
    if (!this.state.message.content.do_not_autoblock && !event.target.dataset.do_not_autoblock) {
      ASRouterUtils.blockById(this.state.message.id);
    }
    if (this.state.message.provider !== "preview") {
      this.sendUserActionTelemetry({event: "CLICK_BUTTON", ...metric});
    }
  }

  onBlockById(id) {
    return options => ASRouterUtils.blockById(id, options);
  }

  onDismissById(id) {
    return () => ASRouterUtils.dismissById(id);
  }

  dismissBundle(bundle) {
    return () => ASRouterUtils.dismissBundle(bundle);
  }

  triggerOnboarding() {
    ASRouterUtils.sendMessage({type: "TRIGGER", data: {trigger: {id: "showOnboarding"}}});
  }

  onMessageFromParent({data: action}) {
    switch (action.type) {
      case "SET_MESSAGE":
        this.setState({message: action.data});
        break;
      case "SET_BUNDLED_MESSAGES":
        this.setState({bundle: action.data});
        break;
      case "CLEAR_MESSAGE":
        if (action.data.id === this.state.message.id) {
          this.setState({message: {}});
          // Remove any styles related to the RTAMO message
          document.body.classList.remove("welcome", "hide-main", "amo");
        }
        break;
      case "CLEAR_PROVIDER":
        if (action.data.id === this.state.message.provider) {
          this.setState({message: {}});
        }
        break;
      case "CLEAR_BUNDLE":
        if (this.state.bundle.bundle) {
          this.setState({bundle: {}});
        }
        break;
      case "CLEAR_ALL":
        this.setState({message: {}, bundle: {}});
    }
  }

  componentWillMount() {
    if (global.document) {
      // Add locale data for StartupOverlay because it uses react-intl
      addLocaleData(global.document.documentElement.lang);
    }

    const endpoint = ASRouterUtils.getPreviewEndpoint();
    ASRouterUtils.addListener(this.onMessageFromParent);

    // If we are loading about:welcome we want to trigger the onboarding messages
    if (this.props.document && this.props.document.location.href === "about:welcome") {
      ASRouterUtils.sendMessage({type: "TRIGGER", data: {trigger: {id: "firstRun"}}});
    } else {
      ASRouterUtils.sendMessage({type: "SNIPPETS_REQUEST", data: {endpoint}});
    }
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  renderSnippets() {
    if (this.state.bundle.template === "onboarding" ||
        this.state.message.template === "fxa_overlay" ||
        this.state.message.template === "return_to_amo_overlay") {
      return null;
    }
    const SnippetComponent = SnippetsTemplates[this.state.message.template];
    const {content} = this.state.message;

    return (
      <ImpressionsWrapper
        id="NEWTAB_FOOTER_BAR"
        message={this.state.message}
        sendImpression={this.sendImpression}
        shouldSendImpressionOnUpdate={shouldSendImpressionOnUpdate}
        // This helps with testing
        document={this.props.document}>
          <LocalizationProvider messages={generateMessages(content)}>
            <SnippetComponent
              {...this.state.message}
              UISurface="NEWTAB_FOOTER_BAR"
              onBlock={this.onBlockById(this.state.message.id)}
              onDismiss={this.onDismissById(this.state.message.id)}
              onAction={ASRouterUtils.executeAction}
              sendClick={this.sendClick}
              sendUserActionTelemetry={this.sendUserActionTelemetry} />
          </LocalizationProvider>
      </ImpressionsWrapper>);
  }

  renderOnboarding() {
    if (this.state.bundle.template === "onboarding") {
      return (
        <OnboardingMessage
          {...this.state.bundle}
          UISurface="NEWTAB_OVERLAY"
          onAction={ASRouterUtils.executeAction}
          onDoneButton={this.dismissBundle(this.state.bundle.bundle)}
          sendUserActionTelemetry={this.sendUserActionTelemetry} />);
    }
    return null;
  }

  renderFirstRunOverlay() {
    const {message} = this.state;
    if (message.template === "fxa_overlay") {
      global.document.body.classList.add("fxa");
      return (
        <IntlProvider locale={global.document.documentElement.lang} messages={global.gActivityStreamStrings}>
          <StartupOverlay
            onReady={this.triggerOnboarding}
            onBlock={this.onDismissById(message.id)}
            dispatch={this.props.dispatch} />
        </IntlProvider>
      );
    } else if (message.template === "return_to_amo_overlay") {
      global.document.body.classList.add("amo");
      return (
        <LocalizationProvider messages={generateMessages({"amo_html": message.content.text})}>
          <ReturnToAMO
            {...message}
            onReady={this.triggerOnboarding}
            onBlock={this.onDismissById(message.id)}
            onAction={ASRouterUtils.executeAction} />
        </LocalizationProvider>
      );
    }
    return null;
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

  render() {
    const {message, bundle} = this.state;
    if (!message.id && !bundle.template) { return null; }
    const shouldRenderBelowSearch = TEMPLATES_BELOW_SEARCH.includes(message.template);

    return shouldRenderBelowSearch ?
      // Render special below search snippets in place;
      <div className="below-search-snippet">{this.renderSnippets()}</div> :
      // For onboarding, regular snippets etc. we should render
      // everything in our footer container.
      ReactDOM.createPortal(
        <React.Fragment>
          {this.renderPreviewBanner()}
          {this.renderFirstRunOverlay()}
          {this.renderOnboarding()}
          {this.renderSnippets()}
        </React.Fragment>,
        this.portalContainer
      );
  }
}

ASRouterUISurface.defaultProps = {document: global.document};
