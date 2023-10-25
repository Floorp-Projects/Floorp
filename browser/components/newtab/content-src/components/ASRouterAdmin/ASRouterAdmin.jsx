/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "common/Actions.sys.mjs";
import { ASRouterUtils } from "../../asrouter/asrouter-utils";
import { connect } from "react-redux";
import React from "react";
import { SimpleHashRouter } from "./SimpleHashRouter";
import { CopyButton } from "./CopyButton";
import { ImpressionsSection } from "./ImpressionsSection";

const Row = props => (
  <tr className="message-item" {...props}>
    {props.children}
  </tr>
);

function relativeTime(timestamp) {
  if (!timestamp) {
    return "";
  }
  const seconds = Math.floor((Date.now() - timestamp) / 1000);
  const minutes = Math.floor((Date.now() - timestamp) / 60000);
  if (seconds < 2) {
    return "just now";
  } else if (seconds < 60) {
    return `${seconds} seconds ago`;
  } else if (minutes === 1) {
    return "1 minute ago";
  } else if (minutes < 600) {
    return `${minutes} minutes ago`;
  }
  return new Date(timestamp).toLocaleString();
}

export class ToggleStoryButton extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.onClick(this.props.story);
  }

  render() {
    return <button onClick={this.handleClick}>collapse/open</button>;
  }
}

export class ToggleMessageJSON extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    this.props.toggleJSON(this.props.msgId);
  }

  render() {
    let iconName = this.props.isCollapsed
      ? "icon icon-arrowhead-forward-small"
      : "icon icon-arrowhead-down-small";
    return (
      <button className="clearButton" onClick={this.handleClick}>
        <span className={iconName} />
      </button>
    );
  }
}

export class TogglePrefCheckbox extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onChange = this.onChange.bind(this);
  }

  onChange(event) {
    this.props.onChange(this.props.pref, event.target.checked);
  }

  render() {
    return (
      <>
        <input
          type="checkbox"
          checked={this.props.checked}
          onChange={this.onChange}
          disabled={this.props.disabled}
        />{" "}
        {this.props.pref}{" "}
      </>
    );
  }
}

export class Personalization extends React.PureComponent {
  constructor(props) {
    super(props);
    this.togglePersonalization = this.togglePersonalization.bind(this);
  }

  togglePersonalization() {
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.DISCOVERY_STREAM_PERSONALIZATION_TOGGLE,
      })
    );
  }

  render() {
    const { lastUpdated, initialized } = this.props.state.Personalization;
    return (
      <React.Fragment>
        <table>
          <tbody>
            <Row>
              <td colSpan="2">
                <TogglePrefCheckbox
                  checked={this.props.personalized}
                  pref="personalized"
                  onChange={this.togglePersonalization}
                />
              </td>
            </Row>
            <Row>
              <td className="min">Personalization Last Updated</td>
              <td>{relativeTime(lastUpdated) || "(no data)"}</td>
            </Row>
            <Row>
              <td className="min">Personalization Initialized</td>
              <td>{initialized ? "true" : "false"}</td>
            </Row>
          </tbody>
        </table>
      </React.Fragment>
    );
  }
}

export class DiscoveryStreamAdmin extends React.PureComponent {
  constructor(props) {
    super(props);
    this.restorePrefDefaults = this.restorePrefDefaults.bind(this);
    this.setConfigValue = this.setConfigValue.bind(this);
    this.expireCache = this.expireCache.bind(this);
    this.refreshCache = this.refreshCache.bind(this);
    this.idleDaily = this.idleDaily.bind(this);
    this.systemTick = this.systemTick.bind(this);
    this.syncRemoteSettings = this.syncRemoteSettings.bind(this);
    this.onStoryToggle = this.onStoryToggle.bind(this);
    this.state = {
      toggledStories: {},
    };
  }

  setConfigValue(name, value) {
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.DISCOVERY_STREAM_CONFIG_SET_VALUE,
        data: { name, value },
      })
    );
  }

  restorePrefDefaults(event) {
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.DISCOVERY_STREAM_CONFIG_RESET_DEFAULTS,
      })
    );
  }

  refreshCache() {
    const { config } = this.props.state.DiscoveryStream;
    this.props.dispatch(
      ac.OnlyToMain({
        type: at.DISCOVERY_STREAM_CONFIG_CHANGE,
        data: config,
      })
    );
  }

  dispatchSimpleAction(type) {
    this.props.dispatch(
      ac.OnlyToMain({
        type,
      })
    );
  }

  systemTick() {
    this.dispatchSimpleAction(at.DISCOVERY_STREAM_DEV_SYSTEM_TICK);
  }

  expireCache() {
    this.dispatchSimpleAction(at.DISCOVERY_STREAM_DEV_EXPIRE_CACHE);
  }

  idleDaily() {
    this.dispatchSimpleAction(at.DISCOVERY_STREAM_DEV_IDLE_DAILY);
  }

  syncRemoteSettings() {
    this.dispatchSimpleAction(at.DISCOVERY_STREAM_DEV_SYNC_RS);
  }

  renderComponent(width, component) {
    return (
      <table>
        <tbody>
          <Row>
            <td className="min">Type</td>
            <td>{component.type}</td>
          </Row>
          <Row>
            <td className="min">Width</td>
            <td>{width}</td>
          </Row>
          {component.feed && this.renderFeed(component.feed)}
        </tbody>
      </table>
    );
  }

  renderFeedData(url) {
    const { feeds } = this.props.state.DiscoveryStream;
    const feed = feeds.data[url].data;
    return (
      <React.Fragment>
        <h4>Feed url: {url}</h4>
        <table>
          <tbody>
            {feed.recommendations?.map(story => this.renderStoryData(story))}
          </tbody>
        </table>
      </React.Fragment>
    );
  }

  renderFeedsData() {
    const { feeds } = this.props.state.DiscoveryStream;
    return (
      <React.Fragment>
        {Object.keys(feeds.data).map(url => this.renderFeedData(url))}
      </React.Fragment>
    );
  }

  renderSpocs() {
    const { spocs } = this.props.state.DiscoveryStream;
    let spocsData = [];
    if (spocs.data && spocs.data.spocs && spocs.data.spocs.items) {
      spocsData = spocs.data.spocs.items || [];
    }

    return (
      <React.Fragment>
        <table>
          <tbody>
            <Row>
              <td className="min">spocs_endpoint</td>
              <td>{spocs.spocs_endpoint}</td>
            </Row>
            <Row>
              <td className="min">Data last fetched</td>
              <td>{relativeTime(spocs.lastUpdated)}</td>
            </Row>
          </tbody>
        </table>
        <h4>Spoc data</h4>
        <table>
          <tbody>{spocsData.map(spoc => this.renderStoryData(spoc))}</tbody>
        </table>
        <h4>Spoc frequency caps</h4>
        <table>
          <tbody>
            {spocs.frequency_caps.map(spoc => this.renderStoryData(spoc))}
          </tbody>
        </table>
      </React.Fragment>
    );
  }

  onStoryToggle(story) {
    const { toggledStories } = this.state;
    this.setState({
      toggledStories: {
        ...toggledStories,
        [story.id]: !toggledStories[story.id],
      },
    });
  }

  renderStoryData(story) {
    let storyData = "";
    if (this.state.toggledStories[story.id]) {
      storyData = JSON.stringify(story, null, 2);
    }
    return (
      <tr className="message-item" key={story.id}>
        <td className="message-id">
          <span>
            {story.id} <br />
          </span>
          <ToggleStoryButton story={story} onClick={this.onStoryToggle} />
        </td>
        <td className="message-summary">
          <pre>{storyData}</pre>
        </td>
      </tr>
    );
  }

  renderFeed(feed) {
    const { feeds } = this.props.state.DiscoveryStream;
    if (!feed.url) {
      return null;
    }
    return (
      <React.Fragment>
        <Row>
          <td className="min">Feed url</td>
          <td>{feed.url}</td>
        </Row>
        <Row>
          <td className="min">Data last fetched</td>
          <td>
            {relativeTime(
              feeds.data[feed.url] ? feeds.data[feed.url].lastUpdated : null
            ) || "(no data)"}
          </td>
        </Row>
      </React.Fragment>
    );
  }

  render() {
    const prefToggles = "enabled collapsible".split(" ");
    const { config, layout } = this.props.state.DiscoveryStream;
    const personalized =
      this.props.otherPrefs["discoverystream.personalization.enabled"];
    return (
      <div>
        <button className="button" onClick={this.restorePrefDefaults}>
          Restore Pref Defaults
        </button>{" "}
        <button className="button" onClick={this.refreshCache}>
          Refresh Cache
        </button>
        <br />
        <button className="button" onClick={this.expireCache}>
          Expire Cache
        </button>{" "}
        <button className="button" onClick={this.systemTick}>
          Trigger System Tick
        </button>{" "}
        <button className="button" onClick={this.idleDaily}>
          Trigger Idle Daily
        </button>
        <br />
        <button className="button" onClick={this.syncRemoteSettings}>
          Sync Remote Settings
        </button>
        <table>
          <tbody>
            {prefToggles.map(pref => (
              <Row key={pref}>
                <td>
                  <TogglePrefCheckbox
                    checked={config[pref]}
                    pref={pref}
                    onChange={this.setConfigValue}
                  />
                </td>
              </Row>
            ))}
          </tbody>
        </table>
        <h3>Layout</h3>
        {layout.map((row, rowIndex) => (
          <div key={`row-${rowIndex}`}>
            {row.components.map((component, componentIndex) => (
              <div key={`component-${componentIndex}`} className="ds-component">
                {this.renderComponent(row.width, component)}
              </div>
            ))}
          </div>
        ))}
        <h3>Personalization</h3>
        <Personalization
          personalized={personalized}
          dispatch={this.props.dispatch}
          state={{
            Personalization: this.props.state.Personalization,
          }}
        />
        <h3>Spocs</h3>
        {this.renderSpocs()}
        <h3>Feeds Data</h3>
        {this.renderFeedsData()}
      </div>
    );
  }
}

export class ASRouterAdminInner extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleEnabledToggle = this.handleEnabledToggle.bind(this);
    this.handleUserPrefToggle = this.handleUserPrefToggle.bind(this);
    this.onChangeMessageFilter = this.onChangeMessageFilter.bind(this);
    this.onChangeMessageGroupsFilter =
      this.onChangeMessageGroupsFilter.bind(this);
    this.unblockAll = this.unblockAll.bind(this);
    this.handleClearAllImpressionsByProvider =
      this.handleClearAllImpressionsByProvider.bind(this);
    this.handleExpressionEval = this.handleExpressionEval.bind(this);
    this.onChangeTargetingParameters =
      this.onChangeTargetingParameters.bind(this);
    this.onChangeAttributionParameters =
      this.onChangeAttributionParameters.bind(this);
    this.setAttribution = this.setAttribution.bind(this);
    this.onCopyTargetingParams = this.onCopyTargetingParams.bind(this);
    this.onNewTargetingParams = this.onNewTargetingParams.bind(this);
    this.handleOpenPB = this.handleOpenPB.bind(this);
    this.selectPBMessage = this.selectPBMessage.bind(this);
    this.resetPBJSON = this.resetPBJSON.bind(this);
    this.resetPBMessageState = this.resetPBMessageState.bind(this);
    this.toggleJSON = this.toggleJSON.bind(this);
    this.toggleAllMessages = this.toggleAllMessages.bind(this);
    this.resetGroups = this.resetGroups.bind(this);
    this.onMessageFromParent = this.onMessageFromParent.bind(this);
    this.setStateFromParent = this.setStateFromParent.bind(this);
    this.setState = this.setState.bind(this);
    this.state = {
      messageFilter: "all",
      messageGroupsFilter: "all",
      collapsedMessages: [],
      modifiedMessages: [],
      selectedPBMessage: "",
      evaluationStatus: {},
      stringTargetingParameters: null,
      newStringTargetingParameters: null,
      copiedToClipboard: false,
      attributionParameters: {
        source: "addons.mozilla.org",
        medium: "referral",
        campaign: "non-fx-button",
        content: `rta:${btoa("uBlock0@raymondhill.net")}`,
        experiment: "ua-onboarding",
        variation: "chrome",
        ua: "Google Chrome 123",
        dltoken: "00000000-0000-0000-0000-000000000000",
      },
    };
  }

  onMessageFromParent({ type, data }) {
    // These only exists due to onPrefChange events in ASRouter
    switch (type) {
      case "UpdateAdminState": {
        this.setStateFromParent(data);
        break;
      }
    }
  }

  setStateFromParent(data) {
    this.setState(data);
    if (!this.state.stringTargetingParameters) {
      const stringTargetingParameters = {};
      for (const param of Object.keys(data.targetingParameters)) {
        stringTargetingParameters[param] = JSON.stringify(
          data.targetingParameters[param],
          null,
          2
        );
      }
      this.setState({ stringTargetingParameters });
    }
  }

  componentWillMount() {
    ASRouterUtils.addListener(this.onMessageFromParent);
    const endpoint = ASRouterUtils.getPreviewEndpoint();
    ASRouterUtils.sendMessage({
      type: "ADMIN_CONNECT_STATE",
      data: { endpoint },
    }).then(this.setStateFromParent);
  }

  handleBlock(msg) {
    return () => ASRouterUtils.blockById(msg.id);
  }

  handleUnblock(msg) {
    return () => ASRouterUtils.unblockById(msg.id);
  }

  resetJSON(msg) {
    // reset the displayed JSON for the given message
    document.getElementById(`${msg.id}-textarea`).value = JSON.stringify(
      msg,
      null,
      2
    );
    // remove the message from the list of modified IDs
    let index = this.state.modifiedMessages.indexOf(msg.id);
    this.setState(prevState => ({
      modifiedMessages: [
        ...prevState.modifiedMessages.slice(0, index),
        ...prevState.modifiedMessages.slice(index + 1),
      ],
    }));
  }

  handleOverride(id) {
    return () =>
      ASRouterUtils.overrideMessage(id).then(state => {
        this.setStateFromParent(state);
        this.props.notifyContent({
          message: state.message,
        });
      });
  }

  resetPBMessageState() {
    // Iterate over Private Browsing messages and block/unblock each one to clear impressions
    const PBMessages = this.state.messages.filter(
      message => message.template === "pb_newtab"
    ); // messages from state go here

    PBMessages.forEach(message => {
      if (message?.id) {
        ASRouterUtils.blockById(message.id);
        ASRouterUtils.unblockById(message.id);
      }
    });
    // Clear the selected messages & radio buttons
    document.getElementById("clear radio").checked = true;
    this.selectPBMessage("clear");
  }

  resetPBJSON(msg) {
    // reset the displayed JSON for the given message
    document.getElementById(`${msg.id}-textarea`).value = JSON.stringify(
      msg,
      null,
      2
    );
  }

  handleOpenPB() {
    ASRouterUtils.sendMessage({
      type: "FORCE_PRIVATE_BROWSING_WINDOW",
      data: { message: { content: this.state.selectedPBMessage } },
    });
  }

  expireCache() {
    ASRouterUtils.sendMessage({ type: "EXPIRE_QUERY_CACHE" });
  }

  resetPref() {
    ASRouterUtils.sendMessage({ type: "RESET_PROVIDER_PREF" });
  }

  resetGroups(id, value) {
    ASRouterUtils.sendMessage({
      type: "RESET_GROUPS_STATE",
    }).then(this.setStateFromParent);
  }

  handleExpressionEval() {
    const context = {};
    for (const param of Object.keys(this.state.stringTargetingParameters)) {
      const value = this.state.stringTargetingParameters[param];
      context[param] = value ? JSON.parse(value) : null;
    }
    ASRouterUtils.sendMessage({
      type: "EVALUATE_JEXL_EXPRESSION",
      data: {
        expression: this.refs.expressionInput.value,
        context,
      },
    }).then(this.setStateFromParent);
  }

  onChangeTargetingParameters(event) {
    const { name } = event.target;
    const { value } = event.target;

    this.setState(({ stringTargetingParameters }) => {
      let targetingParametersError = null;
      const updatedParameters = { ...stringTargetingParameters };
      updatedParameters[name] = value;
      try {
        JSON.parse(value);
      } catch (e) {
        console.error(`Error parsing value of parameter ${name}`);
        targetingParametersError = { id: name };
      }

      return {
        copiedToClipboard: false,
        evaluationStatus: {},
        stringTargetingParameters: updatedParameters,
        targetingParametersError,
      };
    });
  }

  unblockAll() {
    return ASRouterUtils.sendMessage({
      type: "UNBLOCK_ALL",
    }).then(this.setStateFromParent);
  }

  handleClearAllImpressionsByProvider() {
    const providerId = this.state.messageFilter;
    if (!providerId) {
      return;
    }
    const userPrefInfo = this.state.userPrefs;

    const isUserEnabled =
      providerId in userPrefInfo ? userPrefInfo[providerId] : true;

    ASRouterUtils.sendMessage({
      type: "DISABLE_PROVIDER",
      data: providerId,
    });
    if (!isUserEnabled) {
      ASRouterUtils.sendMessage({
        type: "SET_PROVIDER_USER_PREF",
        data: { id: providerId, value: true },
      });
    }
    ASRouterUtils.sendMessage({
      type: "ENABLE_PROVIDER",
      data: providerId,
    });
  }

  handleEnabledToggle(event) {
    const provider = this.state.providerPrefs.find(
      p => p.id === event.target.dataset.provider
    );
    const userPrefInfo = this.state.userPrefs;

    const isUserEnabled =
      provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
    const isSystemEnabled = provider.enabled;
    const isEnabling = event.target.checked;

    if (isEnabling) {
      if (!isUserEnabled) {
        ASRouterUtils.sendMessage({
          type: "SET_PROVIDER_USER_PREF",
          data: { id: provider.id, value: true },
        });
      }
      if (!isSystemEnabled) {
        ASRouterUtils.sendMessage({
          type: "ENABLE_PROVIDER",
          data: provider.id,
        });
      }
    } else {
      ASRouterUtils.sendMessage({
        type: "DISABLE_PROVIDER",
        data: provider.id,
      });
    }

    this.setState({ messageFilter: "all" });
  }

  handleUserPrefToggle(event) {
    const action = {
      type: "SET_PROVIDER_USER_PREF",
      data: { id: event.target.dataset.provider, value: event.target.checked },
    };
    ASRouterUtils.sendMessage(action);
    this.setState({ messageFilter: "all" });
  }

  onChangeMessageFilter(event) {
    this.setState({ messageFilter: event.target.value });
  }

  onChangeMessageGroupsFilter(event) {
    this.setState({ messageGroupsFilter: event.target.value });
  }

  // Simulate a copy event that sets to clipboard all targeting paramters and values
  onCopyTargetingParams(event) {
    const stringTargetingParameters = {
      ...this.state.stringTargetingParameters,
    };
    for (const key of Object.keys(stringTargetingParameters)) {
      // If the value is not set the parameter will be lost when we stringify
      if (stringTargetingParameters[key] === undefined) {
        stringTargetingParameters[key] = null;
      }
    }
    const setClipboardData = e => {
      e.preventDefault();
      e.clipboardData.setData(
        "text",
        JSON.stringify(stringTargetingParameters, null, 2)
      );
      document.removeEventListener("copy", setClipboardData);
      this.setState({ copiedToClipboard: true });
    };

    document.addEventListener("copy", setClipboardData);

    document.execCommand("copy");
  }

  onNewTargetingParams(event) {
    this.setState({ newStringTargetingParameters: event.target.value });
    event.target.classList.remove("errorState");
    this.refs.targetingParamsEval.innerText = "";

    try {
      const stringTargetingParameters = JSON.parse(event.target.value);
      this.setState({ stringTargetingParameters });
    } catch (e) {
      event.target.classList.add("errorState");
      this.refs.targetingParamsEval.innerText = e.message;
    }
  }

  toggleJSON(msgId) {
    if (this.state.collapsedMessages.includes(msgId)) {
      let index = this.state.collapsedMessages.indexOf(msgId);
      this.setState(prevState => ({
        collapsedMessages: [
          ...prevState.collapsedMessages.slice(0, index),
          ...prevState.collapsedMessages.slice(index + 1),
        ],
      }));
    } else {
      this.setState(prevState => ({
        collapsedMessages: prevState.collapsedMessages.concat(msgId),
      }));
    }
  }

  handleChange(msgId) {
    if (!this.state.modifiedMessages.includes(msgId)) {
      this.setState(prevState => ({
        modifiedMessages: prevState.modifiedMessages.concat(msgId),
      }));
    }
  }

  renderMessageItem(msg) {
    const isBlockedByGroup = this.state.groups
      .filter(group => msg.groups.includes(group.id))
      .some(group => !group.enabled);
    const msgProvider =
      this.state.providers.find(provider => provider.id === msg.provider) || {};
    const isProviderExcluded =
      msgProvider.exclude && msgProvider.exclude.includes(msg.id);
    const isMessageBlocked =
      this.state.messageBlockList.includes(msg.id) ||
      this.state.messageBlockList.includes(msg.campaign);
    const isBlocked =
      isMessageBlocked || isBlockedByGroup || isProviderExcluded;
    const impressions = this.state.messageImpressions[msg.id]
      ? this.state.messageImpressions[msg.id].length
      : 0;
    const isCollapsed = this.state.collapsedMessages.includes(msg.id);
    const isModified = this.state.modifiedMessages.includes(msg.id);
    const aboutMessagePreviewSupported = [
      "infobar",
      "spotlight",
      "cfr_doorhanger",
    ].includes(msg.template);

    let itemClassName = "message-item";
    if (isBlocked) {
      itemClassName += " blocked";
    }

    return (
      <tr className={itemClassName} key={`${msg.id}-${msg.provider}`}>
        <td className="message-id">
          <span>
            {msg.id} <br />
          </span>
        </td>
        <td>
          <ToggleMessageJSON
            msgId={`${msg.id}`}
            toggleJSON={this.toggleJSON}
            isCollapsed={isCollapsed}
          />
        </td>
        <td className="button-column">
          <button
            className={`button ${isBlocked ? "" : " primary"}`}
            onClick={
              isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)
            }
          >
            {isBlocked ? "Unblock" : "Block"}
          </button>
          {
            // eslint-disable-next-line no-nested-ternary
            isBlocked ? null : isModified ? (
              <button
                className="button restore"
                // eslint-disable-next-line react/jsx-no-bind
                onClick={e => this.resetJSON(msg)}
              >
                Reset
              </button>
            ) : (
              <button
                className="button show"
                onClick={this.handleOverride(msg.id)}
              >
                Show
              </button>
            )
          }
          {isBlocked ? null : (
            <button
              className="button modify"
              // eslint-disable-next-line react/jsx-no-bind
              onClick={e => this.modifyJson(msg)}
            >
              Modify
            </button>
          )}
          {aboutMessagePreviewSupported ? (
            <CopyButton
              transformer={text =>
                `about:messagepreview?json=${encodeURIComponent(btoa(text))}`
              }
              label="Share"
              copiedLabel="Copied!"
              inputSelector={`#${msg.id}-textarea`}
              className={"button share"}
            />
          ) : null}
          <br />({impressions} impressions)
        </td>
        <td className="message-summary">
          {isBlocked && (
            <tr>
              Block reason:
              {isBlockedByGroup && " Blocked by group"}
              {isProviderExcluded && " Excluded by provider"}
              {isMessageBlocked && " Message blocked"}
            </tr>
          )}
          <tr>
            <pre className={isCollapsed ? "collapsed" : "expanded"}>
              <textarea
                id={`${msg.id}-textarea`}
                name={msg.id}
                className="general-textarea"
                disabled={isBlocked}
                // eslint-disable-next-line react/jsx-no-bind
                onChange={e => this.handleChange(msg.id)}
              >
                {JSON.stringify(msg, null, 2)}
              </textarea>
            </pre>
          </tr>
        </td>
      </tr>
    );
  }

  selectPBMessage(msgId) {
    if (msgId === "clear") {
      this.setState({
        selectedPBMessage: "",
      });
    } else {
      let selected = document.getElementById(`${msgId} radio`);
      let msg = JSON.parse(document.getElementById(`${msgId}-textarea`).value);

      if (selected.checked) {
        this.setState({
          selectedPBMessage: msg?.content,
        });
      } else {
        this.setState({
          selectedPBMessage: "",
        });
      }
    }
  }

  modifyJson(content) {
    const message = JSON.parse(
      document.getElementById(`${content.id}-textarea`).value
    );
    return ASRouterUtils.modifyMessageJson(message).then(state => {
      this.setStateFromParent(state);
      this.props.notifyContent({
        message: state.message,
      });
    });
  }

  renderPBMessageItem(msg) {
    const isBlocked =
      this.state.messageBlockList.includes(msg.id) ||
      this.state.messageBlockList.includes(msg.campaign);
    const impressions = this.state.messageImpressions[msg.id]
      ? this.state.messageImpressions[msg.id].length
      : 0;

    const isCollapsed = this.state.collapsedMessages.includes(msg.id);

    let itemClassName = "message-item";
    if (isBlocked) {
      itemClassName += " blocked";
    }

    return (
      <tr className={itemClassName} key={`${msg.id}-${msg.provider}`}>
        <td className="message-id">
          <span>
            {msg.id} <br />
            <br />({impressions} impressions)
          </span>
        </td>
        <td>
          <ToggleMessageJSON
            msgId={`${msg.id}`}
            toggleJSON={this.toggleJSON}
            isCollapsed={isCollapsed}
          />
        </td>
        <td>
          <input
            type="radio"
            id={`${msg.id} radio`}
            name="PB_message_radio"
            style={{ marginBottom: 20 }}
            onClick={() => this.selectPBMessage(msg.id)}
            disabled={isBlocked}
          />
          <button
            className={`button ${isBlocked ? "" : " primary"}`}
            onClick={
              isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)
            }
          >
            {isBlocked ? "Unblock" : "Block"}
          </button>
          <button
            className="ASRouterButton slim button"
            onClick={e => this.resetPBJSON(msg)}
          >
            Reset JSON
          </button>
        </td>
        <td className={`message-summary`}>
          <pre className={isCollapsed ? "collapsed" : "expanded"}>
            <textarea
              id={`${msg.id}-textarea`}
              className="wnp-textarea"
              name={msg.id}
            >
              {JSON.stringify(msg, null, 2)}
            </textarea>
          </pre>
        </td>
      </tr>
    );
  }

  toggleAllMessages(messagesToShow) {
    if (this.state.collapsedMessages.length) {
      this.setState({
        collapsedMessages: [],
      });
    } else {
      Array.prototype.forEach.call(messagesToShow, msg => {
        this.setState(prevState => ({
          collapsedMessages: prevState.collapsedMessages.concat(msg.id),
        }));
      });
    }
  }

  renderMessages() {
    if (!this.state.messages) {
      return null;
    }
    const messagesToShow =
      this.state.messageFilter === "all"
        ? this.state.messages
        : this.state.messages.filter(
            message =>
              message.provider === this.state.messageFilter &&
              message.template !== "pb_newtab"
          );

    return (
      <div>
        <button
          className="ASRouterButton slim"
          // eslint-disable-next-line react/jsx-no-bind
          onClick={e => this.toggleAllMessages(messagesToShow)}
        >
          Collapse/Expand All
        </button>
        <p className="helpLink">
          <span className="icon icon-small-spacer icon-info" />{" "}
          <span>
            To modify a message, change the JSON and click 'Modify' to see your
            changes. Click 'Reset' to restore the JSON to the original. Click
            'Share' to copy a link to the clipboard that can be used to preview
            the message by opening the link in Nightly/local builds.
          </span>
        </p>
        <table>
          <tbody>
            {messagesToShow.map(msg => this.renderMessageItem(msg))}
          </tbody>
        </table>
      </div>
    );
  }

  renderMessagesByGroup() {
    if (!this.state.messages) {
      return null;
    }
    const messagesToShow =
      this.state.messageGroupsFilter === "all"
        ? this.state.messages.filter(m => m.groups.length)
        : this.state.messages.filter(message =>
            message.groups.includes(this.state.messageGroupsFilter)
          );

    return (
      <table>
        <tbody>{messagesToShow.map(msg => this.renderMessageItem(msg))}</tbody>
      </table>
    );
  }

  renderPBMessages() {
    if (!this.state.messages) {
      return null;
    }
    const messagesToShow = this.state.messages.filter(
      message => message.template === "pb_newtab"
    );
    return (
      <table>
        <tbody>
          {messagesToShow.map(msg => this.renderPBMessageItem(msg))}
        </tbody>
      </table>
    );
  }

  renderMessageFilter() {
    if (!this.state.providers) {
      return null;
    }

    return (
      <p>
        <button
          className="unblock-all ASRouterButton test-only"
          onClick={this.unblockAll}
        >
          Unblock All Snippets
        </button>
        Show messages from{" "}
        <select
          value={this.state.messageFilter}
          onChange={this.onChangeMessageFilter}
        >
          <option value="all">all providers</option>
          {this.state.providers.map(provider => (
            <option key={provider.id} value={provider.id}>
              {provider.id}
            </option>
          ))}
        </select>
        {this.state.messageFilter !== "all" &&
        !this.state.messageFilter.includes("_local_testing") ? (
          <button
            className="button messages-reset"
            onClick={this.handleClearAllImpressionsByProvider}
          >
            Reset All
          </button>
        ) : null}
      </p>
    );
  }

  renderMessageGroupsFilter() {
    if (!this.state.groups) {
      return null;
    }

    return (
      <p>
        Show messages from {/* eslint-disable-next-line jsx-a11y/no-onchange */}
        <select
          value={this.state.messageGroupsFilter}
          onChange={this.onChangeMessageGroupsFilter}
        >
          <option value="all">all groups</option>
          {this.state.groups.map(group => (
            <option key={group.id} value={group.id}>
              {group.id}
            </option>
          ))}
        </select>
      </p>
    );
  }

  renderTableHead() {
    return (
      <thead>
        <tr className="message-item">
          <td className="min" />
          <td className="min">Provider ID</td>
          <td>Source</td>
          <td className="min">Cohort</td>
          <td className="min">Last Updated</td>
        </tr>
      </thead>
    );
  }

  renderProviders() {
    const providersConfig = this.state.providerPrefs;
    const providerInfo = this.state.providers;
    const userPrefInfo = this.state.userPrefs;

    return (
      <table>
        {this.renderTableHead()}
        <tbody>
          {providersConfig.map((provider, i) => {
            const isTestProvider = provider.id.includes("_local_testing");
            const info = providerInfo.find(p => p.id === provider.id) || {};
            const isUserEnabled =
              provider.id in userPrefInfo ? userPrefInfo[provider.id] : true;
            const isSystemEnabled = isTestProvider || provider.enabled;

            let label = "local";
            if (provider.type === "remote") {
              label = (
                <span>
                  endpoint (
                  <a
                    className="providerUrl"
                    target="_blank"
                    href={info.url}
                    rel="noopener noreferrer"
                  >
                    {info.url}
                  </a>
                  )
                </span>
              );
            } else if (provider.type === "remote-settings") {
              label = `remote settings (${provider.collection})`;
            } else if (provider.type === "remote-experiments") {
              label = (
                <span>
                  remote settings (
                  <a
                    className="providerUrl"
                    target="_blank"
                    href="https://firefox.settings.services.mozilla.com/v1/buckets/main/collections/nimbus-desktop-experiments/records"
                    rel="noopener noreferrer"
                  >
                    nimbus-desktop-experiments
                  </a>
                  )
                </span>
              );
            }

            let reasonsDisabled = [];
            if (!isSystemEnabled) {
              reasonsDisabled.push("system pref");
            }
            if (!isUserEnabled) {
              reasonsDisabled.push("user pref");
            }
            if (reasonsDisabled.length) {
              label = `disabled via ${reasonsDisabled.join(", ")}`;
            }

            return (
              <tr className="message-item" key={i}>
                <td>
                  {isTestProvider ? (
                    <input
                      type="checkbox"
                      disabled={true}
                      readOnly={true}
                      checked={true}
                    />
                  ) : (
                    <input
                      type="checkbox"
                      data-provider={provider.id}
                      checked={isUserEnabled && isSystemEnabled}
                      onChange={this.handleEnabledToggle}
                    />
                  )}
                </td>
                <td>{provider.id}</td>
                <td>
                  <span
                    className={`sourceLabel${
                      isUserEnabled && isSystemEnabled ? "" : " isDisabled"
                    }`}
                  >
                    {label}
                  </span>
                </td>
                <td>{provider.cohort}</td>
                <td style={{ whiteSpace: "nowrap" }}>
                  {info.lastUpdated
                    ? new Date(info.lastUpdated).toLocaleString()
                    : ""}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    );
  }

  renderTargetingParameters() {
    // There was no error and the result is truthy
    const success =
      this.state.evaluationStatus.success &&
      !!this.state.evaluationStatus.result;
    const result =
      JSON.stringify(this.state.evaluationStatus.result, null, 2) ||
      "(Empty result)";

    return (
      <table>
        <tbody>
          <tr>
            <td>
              <h2>Evaluate JEXL expression</h2>
            </td>
          </tr>
          <tr>
            <td>
              <p>
                <textarea
                  ref="expressionInput"
                  rows="10"
                  cols="60"
                  placeholder="Evaluate JEXL expressions and mock parameters by changing their values below"
                />
              </p>
              <p>
                Status:{" "}
                <span ref="evaluationStatus">
                  {success ? "✅" : "❌"}, Result: {result}
                </span>
              </p>
            </td>
            <td>
              <button
                className="ASRouterButton secondary"
                onClick={this.handleExpressionEval}
              >
                Evaluate
              </button>
            </td>
          </tr>
          <tr>
            <td>
              <h2>Modify targeting parameters</h2>
            </td>
          </tr>
          <tr>
            <td>
              <button
                className="ASRouterButton secondary"
                onClick={this.onCopyTargetingParams}
                disabled={this.state.copiedToClipboard}
              >
                {this.state.copiedToClipboard
                  ? "Parameters copied!"
                  : "Copy parameters"}
              </button>
            </td>
          </tr>
          {this.state.stringTargetingParameters &&
            Object.keys(this.state.stringTargetingParameters).map(
              (param, i) => {
                const value = this.state.stringTargetingParameters[param];
                const errorState =
                  this.state.targetingParametersError &&
                  this.state.targetingParametersError.id === param;
                const className = errorState ? "errorState" : "";
                const inputComp =
                  (value && value.length) > 30 ? (
                    <textarea
                      name={param}
                      className={className}
                      value={value}
                      rows="10"
                      cols="60"
                      onChange={this.onChangeTargetingParameters}
                    />
                  ) : (
                    <input
                      name={param}
                      className={className}
                      value={value}
                      onChange={this.onChangeTargetingParameters}
                    />
                  );

                return (
                  <tr key={i}>
                    <td>{param}</td>
                    <td>{inputComp}</td>
                  </tr>
                );
              }
            )}
        </tbody>
      </table>
    );
  }

  onChangeAttributionParameters(event) {
    const { name, value } = event.target;

    this.setState(({ attributionParameters }) => {
      const updatedParameters = { ...attributionParameters };
      updatedParameters[name] = value;

      return { attributionParameters: updatedParameters };
    });
  }

  setAttribution(e) {
    ASRouterUtils.sendMessage({
      type: "FORCE_ATTRIBUTION",
      data: this.state.attributionParameters,
    }).then(this.setStateFromParent);
  }

  _getGroupImpressionsCount(id, frequency) {
    if (frequency) {
      return this.state.groupImpressions[id]
        ? this.state.groupImpressions[id].length
        : 0;
    }

    return "n/a";
  }

  renderAttributionParamers() {
    return (
      <div>
        <h2> Attribution Parameters </h2>
        <p>
          {" "}
          This forces the browser to set some attribution parameters, useful for
          testing the Return To AMO feature. Clicking on 'Force Attribution',
          with the default values in each field, will demo the Return To AMO
          flow with the addon called 'uBlock Origin'. If you wish to try
          different attribution parameters, enter them in the text boxes. If you
          wish to try a different addon with the Return To AMO flow, make sure
          the 'content' text box has a string that is 'rta:base64(addonID)', the
          base64 string of the addonID prefixed with 'rta:'. The addon must
          currently be a recommended addon on AMO. Then click 'Force
          Attribution'. Clicking on 'Force Attribution' with blank text boxes
          reset attribution data.
        </p>
        <table>
          <tr>
            <td>
              <b> Source </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="source"
                placeholder="addons.mozilla.org"
                value={this.state.attributionParameters.source}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> Medium </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="medium"
                placeholder="referral"
                value={this.state.attributionParameters.medium}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> Campaign </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="campaign"
                placeholder="non-fx-button"
                value={this.state.attributionParameters.campaign}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> Content </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="content"
                placeholder={`rta:${btoa("uBlock0@raymondhill.net")}`}
                value={this.state.attributionParameters.content}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> Experiment </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="experiment"
                placeholder="ua-onboarding"
                value={this.state.attributionParameters.experiment}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> Variation </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="variation"
                placeholder="chrome"
                value={this.state.attributionParameters.variation}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> User Agent </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="ua"
                placeholder="Google Chrome 123"
                value={this.state.attributionParameters.ua}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              <b> Download Token </b>
            </td>
            <td>
              {" "}
              <input
                type="text"
                name="dltoken"
                placeholder="00000000-0000-0000-0000-000000000000"
                value={this.state.attributionParameters.dltoken}
                onChange={this.onChangeAttributionParameters}
              />{" "}
            </td>
          </tr>
          <tr>
            <td>
              {" "}
              <button
                className="ASRouterButton primary button"
                onClick={this.setAttribution}
              >
                {" "}
                Force Attribution{" "}
              </button>{" "}
            </td>
          </tr>
        </table>
      </div>
    );
  }

  renderErrorMessage({ id, errors }) {
    const providerId = <td rowSpan={errors.length}>{id}</td>;
    // .reverse() so that the last error (most recent) is first
    return errors
      .map(({ error, timestamp }, cellKey) => (
        <tr key={cellKey}>
          {cellKey === errors.length - 1 ? providerId : null}
          <td>{error.message}</td>
          <td>{relativeTime(timestamp)}</td>
        </tr>
      ))
      .reverse();
  }

  renderErrors() {
    const providersWithErrors =
      this.state.providers &&
      this.state.providers.filter(p => p.errors && p.errors.length);

    if (providersWithErrors && providersWithErrors.length) {
      return (
        <table className="errorReporting">
          <thead>
            <tr>
              <th>Provider ID</th>
              <th>Message</th>
              <th>Timestamp</th>
            </tr>
          </thead>
          <tbody>{providersWithErrors.map(this.renderErrorMessage)}</tbody>
        </table>
      );
    }

    return <p>No errors</p>;
  }

  renderPBTab() {
    if (!this.state.messages) {
      return null;
    }
    let messagesToShow = this.state.messages.filter(
      message => message.template === "pb_newtab"
    );

    return (
      <div>
        <p className="helpLink">
          <span className="icon icon-small-spacer icon-info" />{" "}
          <span>
            To view an available message, select its radio button and click
            "Open a Private Browsing Window".
            <br />
            To modify a message, make changes to the JSON first, then select the
            radio button. (To make new changes, click "Reset Message State",
            make your changes, and reselect the radio button.)
            <br />
            Click "Reset Message State" to clear all message impressions and
            view messages in a clean state.
            <br />
            Note that ContentSearch functions do not work in debug mode.
          </span>
        </p>
        <div>
          <button
            className="ASRouterButton primary button"
            onClick={this.handleOpenPB}
          >
            Open a Private Browsing Window
          </button>
          <button
            className="ASRouterButton primary button"
            style={{ marginInlineStart: 12 }}
            onClick={this.resetPBMessageState}
          >
            Reset Message State
          </button>
          <br />
          <input
            type="radio"
            id={`clear radio`}
            name="PB_message_radio"
            value="clearPBMessage"
            style={{ display: "none" }}
          />
          <h2>Messages</h2>
          <button
            className="ASRouterButton slim button"
            // eslint-disable-next-line react/jsx-no-bind
            onClick={e => this.toggleAllMessages(messagesToShow)}
          >
            Collapse/Expand All
          </button>
          {this.renderPBMessages()}
        </div>
      </div>
    );
  }

  getSection() {
    const [section] = this.props.location.routes;
    switch (section) {
      case "private":
        return (
          <React.Fragment>
            <h2>Private Browsing Messages</h2>
            {this.renderPBTab()}
          </React.Fragment>
        );
      case "targeting":
        return (
          <React.Fragment>
            <h2>Targeting Utilities</h2>
            <button className="button" onClick={this.expireCache}>
              Expire Cache
            </button>{" "}
            (This expires the cache in ASR Targeting for bookmarks and top
            sites)
            {this.renderTargetingParameters()}
            {this.renderAttributionParamers()}
          </React.Fragment>
        );
      case "groups":
        return (
          <React.Fragment>
            <h2>Message Groups</h2>
            <button className="button" onClick={this.resetGroups}>
              Reset group impressions
            </button>
            <table>
              <thead>
                <tr className="message-item">
                  <td>Enabled</td>
                  <td>Impressions count</td>
                  <td>Custom frequency</td>
                  <td>User preferences</td>
                </tr>
              </thead>
              <tbody>
                {this.state.groups &&
                  this.state.groups.map(
                    (
                      { id, enabled, frequency, userPreferences = [] },
                      index
                    ) => (
                      <Row key={id}>
                        <td>
                          <TogglePrefCheckbox
                            checked={enabled}
                            pref={id}
                            disabled={true}
                          />
                        </td>
                        <td>{this._getGroupImpressionsCount(id, frequency)}</td>
                        <td>{JSON.stringify(frequency, null, 2)}</td>
                        <td>{userPreferences.join(", ")}</td>
                      </Row>
                    )
                  )}
              </tbody>
            </table>
            {this.renderMessageGroupsFilter()}
            {this.renderMessagesByGroup()}
          </React.Fragment>
        );
      case "impressions":
        return (
          <React.Fragment>
            <h2>Impressions</h2>
            <ImpressionsSection
              messageImpressions={this.state.messageImpressions}
              groupImpressions={this.state.groupImpressions}
              screenImpressions={this.state.screenImpressions}
            />
          </React.Fragment>
        );
      case "ds":
        return (
          <React.Fragment>
            <h2>Discovery Stream</h2>
            <DiscoveryStreamAdmin
              state={{
                DiscoveryStream: this.props.DiscoveryStream,
                Personalization: this.props.Personalization,
              }}
              otherPrefs={this.props.Prefs.values}
              dispatch={this.props.dispatch}
            />
          </React.Fragment>
        );
      case "errors":
        return (
          <React.Fragment>
            <h2>ASRouter Errors</h2>
            {this.renderErrors()}
          </React.Fragment>
        );
      default:
        return (
          <React.Fragment>
            <h2>
              Message Providers{" "}
              <button
                title="Restore all provider settings that ship with Firefox"
                className="button"
                onClick={this.resetPref}
              >
                Restore default prefs
              </button>
            </h2>
            {this.state.providers ? this.renderProviders() : null}
            <h2>Messages</h2>
            {this.renderMessageFilter()}
            {this.renderMessages()}
          </React.Fragment>
        );
    }
  }

  render() {
    return (
      <div
        className={`asrouter-admin ${
          this.props.collapsed ? "collapsed" : "expanded"
        }`}
      >
        <aside className="sidebar">
          <ul>
            <li>
              <a href="#devtools">General</a>
            </li>
            <li>
              <a href="#devtools-private">Private Browsing</a>
            </li>
            <li>
              <a href="#devtools-targeting">Targeting</a>
            </li>
            <li>
              <a href="#devtools-groups">Message Groups</a>
            </li>
            <li>
              <a href="#devtools-impressions">Impressions</a>
            </li>
            <li>
              <a href="#devtools-ds">Discovery Stream</a>
            </li>
            <li>
              <a href="#devtools-errors">Errors</a>
            </li>
          </ul>
        </aside>
        <main className="main-panel">
          <h1>AS Router Admin</h1>

          <p className="helpLink">
            <span className="icon icon-small-spacer icon-info" />{" "}
            <span>
              Need help using these tools? Check out our{" "}
              <a
                target="blank"
                href="https://firefox-source-docs.mozilla.org/browser/components/newtab/content-src/asrouter/docs/debugging-docs.html"
              >
                documentation
              </a>
            </span>
          </p>

          {this.getSection()}
        </main>
      </div>
    );
  }
}

export class CollapseToggle extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onCollapseToggle = this.onCollapseToggle.bind(this);
    this.state = { collapsed: false };
  }

  get renderAdmin() {
    const { props } = this;
    return (
      props.location.hash &&
      (props.location.hash.startsWith("#asrouter") ||
        props.location.hash.startsWith("#devtools"))
    );
  }

  onCollapseToggle(e) {
    e.preventDefault();
    this.setState(state => ({ collapsed: !state.collapsed }));
  }

  setBodyClass() {
    if (this.renderAdmin && !this.state.collapsed) {
      global.document.body.classList.add("no-scroll");
    } else {
      global.document.body.classList.remove("no-scroll");
    }
  }

  componentDidMount() {
    this.setBodyClass();
  }

  componentDidUpdate() {
    this.setBodyClass();
  }

  componentWillUnmount() {
    global.document.body.classList.remove("no-scroll");
    ASRouterUtils.removeListener(this.onMessageFromParent);
  }

  render() {
    const { props } = this;
    const { renderAdmin } = this;
    const isCollapsed = this.state.collapsed || !renderAdmin;
    const label = `${isCollapsed ? "Expand" : "Collapse"} devtools`;
    return (
      <React.Fragment>
        <a
          href="#devtools"
          title={label}
          aria-label={label}
          className={`asrouter-toggle ${
            isCollapsed ? "collapsed" : "expanded"
          }`}
          onClick={this.renderAdmin ? this.onCollapseToggle : null}
        >
          <span className="icon icon-devtools" />
        </a>
        {renderAdmin ? (
          <ASRouterAdminInner {...props} collapsed={this.state.collapsed} />
        ) : null}
      </React.Fragment>
    );
  }
}

const _ASRouterAdmin = props => (
  <SimpleHashRouter>
    <CollapseToggle {...props} />
  </SimpleHashRouter>
);

export const ASRouterAdmin = connect(state => ({
  Sections: state.Sections,
  DiscoveryStream: state.DiscoveryStream,
  Personalization: state.Personalization,
  Prefs: state.Prefs,
}))(_ASRouterAdmin);
