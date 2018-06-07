import {actionCreators as ac, ASRouterActions as ra} from "common/Actions.jsm";
import {OUTGOING_MESSAGE_NAME as AS_GENERAL_OUTGOING_MESSAGE_NAME} from "content-src/lib/init-store";
import {ImpressionsWrapper} from "./components/ImpressionsWrapper/ImpressionsWrapper";
import {OnboardingMessage} from "./templates/OnboardingMessage/OnboardingMessage";
import React from "react";
import ReactDOM from "react-dom";
import {SimpleSnippet} from "./templates/SimpleSnippet/SimpleSnippet";

const INCOMING_MESSAGE_NAME = "ASRouter:parent-to-child";
const OUTGOING_MESSAGE_NAME = "ASRouter:child-to-parent";

export const ASRouterUtils = {
  addListener(listener) {
    global.addMessageListener(INCOMING_MESSAGE_NAME, listener);
  },
  removeListener(listener) {
    global.removeMessageListener(INCOMING_MESSAGE_NAME, listener);
  },
  sendMessage(action) {
    global.sendAsyncMessage(OUTGOING_MESSAGE_NAME, action);
  },
  blockById(id) {
    ASRouterUtils.sendMessage({type: "BLOCK_MESSAGE_BY_ID", data: {id}});
  },
  blockBundle(bundle) {
    ASRouterUtils.sendMessage({type: "BLOCK_BUNDLE", data: {bundle}});
  },
  executeAction({button_action, button_action_params}) {
    if (button_action in ra) {
      ASRouterUtils.sendMessage({type: button_action, data: {button_action_params}});
    }
  },
  unblockById(id) {
    ASRouterUtils.sendMessage({type: "UNBLOCK_MESSAGE_BY_ID", data: {id}});
  },
  unblockBundle(bundle) {
    ASRouterUtils.sendMessage({type: "UNBLOCK_BUNDLE", data: {bundle}});
  },
  getNextMessage() {
    ASRouterUtils.sendMessage({type: "GET_NEXT_MESSAGE"});
  },
  overrideMessage(id) {
    ASRouterUtils.sendMessage({type: "OVERRIDE_MESSAGE", data: {id}});
  },
  sendTelemetry(ping) {
    const payload = ac.ASRouterUserEvent(ping);
    global.sendAsyncMessage(AS_GENERAL_OUTGOING_MESSAGE_NAME, payload);
  }
};

// Note: nextProps/prevProps refer to props passed to <ImpressionsWrapper />, not <ASRouterUISurface />
function shouldSendImpressionOnUpdate(nextProps, prevProps) {
  return (nextProps.message.id && (!prevProps.message || prevProps.message.id !== nextProps.message.id));
}

export class ASRouterUISurface extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.sendImpression = this.sendImpression.bind(this);
    this.sendUserActionTelemetry = this.sendUserActionTelemetry.bind(this);
    this.state = {message: {}, bundle: {}};
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
      ...extraProps
    });
  }

  sendImpression(extraProps) {
    this.sendUserActionTelemetry({event: "IMPRESSION", ...extraProps});
  }

  onBlockById(id) {
    return () => ASRouterUtils.blockById(id);
  }

  clearBundle(bundle) {
    return () => ASRouterUtils.blockBundle(bundle);
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
    ASRouterUtils.addListener(this.onMessageFromParent);
    ASRouterUtils.sendMessage({type: "CONNECT_UI_REQUEST"});
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  renderSnippets() {
    return (
      <ImpressionsWrapper
        id="NEWTAB_FOOTER_BAR"
        message={this.state.message}
        sendImpression={this.sendImpression}
        shouldSendImpressionOnUpdate={shouldSendImpressionOnUpdate}
        // This helps with testing
        document={this.props.document}>
          <SimpleSnippet
            {...this.state.message}
            UISurface="NEWTAB_FOOTER_BAR"
            getNextMessage={ASRouterUtils.getNextMessage}
            onBlock={this.onBlockById(this.state.message.id)}
            sendUserActionTelemetry={this.sendUserActionTelemetry} />
      </ImpressionsWrapper>);
  }

  renderOnboarding() {
    return (
      <OnboardingMessage
        {...this.state.bundle}
        UISurface="NEWTAB_OVERLAY"
        onAction={ASRouterUtils.executeAction}
        onDoneButton={this.clearBundle(this.state.bundle.bundle)}
        getNextMessage={ASRouterUtils.getNextMessage}
        sendUserActionTelemetry={this.sendUserActionTelemetry} />);
  }

  render() {
    const {message, bundle} = this.state;
    if (!message.id && !bundle.template) { return null; }
    if (bundle.template === "onboarding") { return this.renderOnboarding(); }
    return this.renderSnippets();
  }
}

ASRouterUISurface.defaultProps = {document: global.document};

export class ASRouterContent {
  constructor() {
    this.initialized = false;
    this.containerElement = null;
  }

  _mount() {
    this.containerElement = global.document.getElementById("snippets-container");
    ReactDOM.render(<ASRouterUISurface />, this.containerElement);
  }

  _unmount() {
    ReactDOM.unmountComponentAtNode(this.containerElement);
  }

  init() {
    this._mount();
    this.initialized = true;
  }

  uninit() {
    if (this.initialized) {
      this._unmount();
      this.initialized = false;
    }
  }
}
