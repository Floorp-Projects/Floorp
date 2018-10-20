import {ASRouterUtils} from "../../asrouter/asrouter-content";
import React from "react";

export class ASRouterAdmin extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onMessage = this.onMessage.bind(this);
    this.handleEnabledToggle = this.handleEnabledToggle.bind(this);
    this.onChangeMessageFilter = this.onChangeMessageFilter.bind(this);
    this.findOtherBundledMessagesOfSameTemplate = this.findOtherBundledMessagesOfSameTemplate.bind(this);
    this.state = {messageFilter: "all"};
  }

  onMessage({data: action}) {
    if (action.type === "ADMIN_SET_STATE") {
      this.setState(action.data);
    }
  }

  componentWillMount() {
    const endpoint = ASRouterUtils.getPreviewEndpoint();
    ASRouterUtils.sendMessage({type: "ADMIN_CONNECT_STATE", data: {endpoint}});
    ASRouterUtils.addListener(this.onMessage);
  }

  componentWillUnmount() {
    ASRouterUtils.removeListener(this.onMessage);
  }

  findOtherBundledMessagesOfSameTemplate(template) {
    return this.state.messages.filter(msg => msg.template === template && msg.bundled);
  }

  handleBlock(msg) {
    if (msg.bundled) {
      // If we are blocking a message that belongs to a bundle, block all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => ASRouterUtils.blockBundle(bundle);
    }
    return () => ASRouterUtils.blockById(msg.id);
  }

  handleUnblock(msg) {
    if (msg.bundled) {
      // If we are unblocking a message that belongs to a bundle, unblock all other messages that are bundled of that same template
      let bundle = this.findOtherBundledMessagesOfSameTemplate(msg.template);
      return () => ASRouterUtils.unblockBundle(bundle);
    }
    return () => ASRouterUtils.unblockById(msg.id);
  }

  handleOverride(id) {
    return () => ASRouterUtils.overrideMessage(id);
  }

  expireCache() {
    ASRouterUtils.sendMessage({type: "EXPIRE_QUERY_CACHE"});
  }

  resetPref() {
    ASRouterUtils.sendMessage({type: "RESET_PROVIDER_PREF"});
  }

  renderMessageItem(msg) {
    const isCurrent = msg.id === this.state.lastMessageId;
    const isBlocked = this.state.messageBlockList.includes(msg.id);
    const impressions = this.state.messageImpressions[msg.id] ? this.state.messageImpressions[msg.id].length : 0;

    let itemClassName = "message-item";
    if (isCurrent) { itemClassName += " current"; }
    if (isBlocked) { itemClassName += " blocked"; }

    return (<tr className={itemClassName} key={msg.id}>
      <td className="message-id"><span>{msg.id} <br /></span></td>
      <td>
        <button className={`button ${(isBlocked ? "" : " primary")}`} onClick={isBlocked ? this.handleUnblock(msg) : this.handleBlock(msg)}>{isBlocked ? "Unblock" : "Block"}</button>
       {isBlocked ? null : <button className="button" onClick={this.handleOverride(msg.id)}>Show</button>}
       <br />({impressions} impressions)
      </td>
      <td className="message-summary">
        <pre>{JSON.stringify(msg, null, 2)}</pre>
      </td>
    </tr>);
  }

  renderMessages() {
    if (!this.state.messages) {
      return null;
    }
    const messagesToShow = this.state.messageFilter === "all" ? this.state.messages : this.state.messages.filter(message => message.provider === this.state.messageFilter);
    return (<table><tbody>
      {messagesToShow.map(msg => this.renderMessageItem(msg))}
    </tbody></table>);
  }

  onChangeMessageFilter(event) {
    this.setState({messageFilter: event.target.value});
  }

  renderMessageFilter() {
    if (!this.state.providers) {
      return null;
    }
    return (<p>Show messages from <select value={this.state.messageFilter} onChange={this.onChangeMessageFilter}>
      <option value="all">all providers</option>
      {this.state.providers.map(provider => (<option key={provider.id} value={provider.id}>{provider.id}</option>))}
    </select></p>);
  }

  renderTableHead() {
    return (<thead>
      <tr className="message-item">
        <td>id</td>
        <td>enabled</td>
        <td>source</td>
        <td>last updated</td>
      </tr>
    </thead>);
  }

  handleEnabledToggle(event) {
    const action = {type: event.target.checked ? "ENABLE_PROVIDER" : "DISABLE_PROVIDER", data: event.target.name};
    ASRouterUtils.sendMessage(action);
    this.setState({messageFilter: "all"});
  }

  renderProviders() {
    const providersConfig = this.state.providerPrefs;
    const providerInfo = this.state.providers;
    return (<table>{this.renderTableHead()}<tbody>
      {providersConfig.map((provider, i) => {
        const isTestProvider = provider.id === "snippets_local_testing";
        const info = providerInfo.find(p => p.id === provider.id) || {};
        let label = "(local)";
        if (provider.type === "remote") {
          label = <a target="_blank" href={info.url}>{info.url}</a>;
        } else if (provider.type === "remote-settings") {
          label = `${provider.bucket} (Remote Settings)`;
        }
        return (<tr className="message-item" key={i}>
          <td>{provider.id}</td>
          <td>{isTestProvider ? null : <input type="checkbox" name={provider.id} checked={provider.enabled} onChange={this.handleEnabledToggle} />}</td>
          <td>{label}</td>
          <td style={{whiteSpace: "nowrap"}}>{info.lastUpdated ? new Date(info.lastUpdated).toLocaleString() : ""}</td>
        </tr>);
      })}
    </tbody></table>);
  }

  render() {
    return (<div className="asrouter-admin outer-wrapper">
      <h1>AS Router Admin</h1>
      <h2>Targeting Utilities</h2>
      <button className="button" onClick={this.expireCache}>Expire Cache</button> (This expires the cache in ASR Targeting for bookmarks and top sites)
      <h2>Message Providers</h2>
      <button className="button" onClick={this.resetPref}>Restore defaults</button>
      {this.state.providers ? this.renderProviders() : null}
      <h2>Messages</h2>
      {this.renderMessageFilter()}
      {this.renderMessages()}
    </div>);
  }
}
