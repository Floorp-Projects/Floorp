/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["fetch"]);
const {ASRouterActions: ra} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {OnboardingMessageProvider} = ChromeUtils.import("resource://activity-stream/lib/OnboardingMessageProvider.jsm", {});

ChromeUtils.defineModuleGetter(this, "ASRouterTargeting",
  "resource://activity-stream/lib/ASRouterTargeting.jsm");

const INCOMING_MESSAGE_NAME = "ASRouter:child-to-parent";
const OUTGOING_MESSAGE_NAME = "ASRouter:parent-to-child";
const ONE_HOUR_IN_MS = 60 * 60 * 1000;
const SNIPPETS_ENDPOINT_PREF = "browser.newtabpage.activity-stream.asrouter.snippetsUrl";
// Note: currently a restart is required when this pref is changed, this will be fixed in Bug 1462114
const SNIPPETS_ENDPOINT = Services.prefs.getStringPref(SNIPPETS_ENDPOINT_PREF,
  "https://activity-stream-icons.services.mozilla.com/v1/messages.json.br");

const MessageLoaderUtils = {
  /**
   * _localLoader - Loads messages for a local provider (i.e. one that lives in mozilla central)
   *
   * @param {obj} provider An AS router provider
   * @param {Array} provider.messages An array of messages
   * @returns {Array} the array of messages
   */
  _localLoader(provider) {
    return provider.messages;
  },

  /**
   * _remoteLoader - Loads messages for a remote provider
   *
   * @param {obj} provider An AS router provider
   * @param {string} provider.url An endpoint that returns an array of messages as JSON
   * @returns {Promise} resolves with an array of messages, or an empty array if none could be fetched
   */
  async _remoteLoader(provider) {
    let remoteMessages = [];
    if (provider.url) {
      try {
        const response = await fetch(provider.url);
        if (
          // Empty response
          response.status !== 204 &&
          (response.ok || response.status === 302)
        ) {
          remoteMessages = (await response.json())
            .messages
            .map(msg => ({...msg, provider_url: provider.url}));
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }
    return remoteMessages;
  },

  /**
   * _getMessageLoader - return the right loading function given the provider's type
   *
   * @param {obj} provider An AS Router provider
   * @returns {func} A loading function
   */
  _getMessageLoader(provider) {
    switch (provider.type) {
      case "remote":
        return this._remoteLoader;
      case "local":
      default:
        return this._localLoader;
    }
  },

  /**
   * shouldProviderUpdate - Given the current time, should a provider update its messages?
   *
   * @param {any} provider An AS Router provider
   * @param {int} provider.updateCycleInMs The number of milliseconds we should wait between updates
   * @param {Date} provider.lastUpdated If the provider has been updated, the time the last update occurred
   * @param {Date} currentTime The time we should check against. (defaults to Date.now())
   * @returns {bool} Should an update happen?
   */
  shouldProviderUpdate(provider, currentTime = Date.now()) {
    return (!(provider.lastUpdated >= 0) || currentTime - provider.lastUpdated > provider.updateCycleInMs);
  },

  /**
   * loadMessagesForProvider - Load messages for a provider, given the provider's type.
   *
   * @param {obj} provider An AS Router provider
   * @param {string} provider.type An AS Router provider type (defaults to "local")
   * @returns {obj} Returns an object with .messages (an array of messages) and .lastUpdated (the time the messages were updated)
   */
  async loadMessagesForProvider(provider) {
    const messages = (await this._getMessageLoader(provider)(provider))
        .map(msg => ({...msg, provider: provider.id}));
    const lastUpdated = Date.now();
    return {messages, lastUpdated};
  }
};

this.MessageLoaderUtils = MessageLoaderUtils;

/**
 * @class _ASRouter - Keeps track of all messages, UI surfaces, and
 * handles blocking, rotation, etc. Inspecting ASRouter.state will
 * tell you what the current displayed message is in all UI surfaces.
 *
 * Note: This is written as a constructor rather than just a plain object
 * so that it can be more easily unit tested.
 */
class _ASRouter {
  constructor(initialState = {}) {
    this.initialized = false;
    this.messageChannel = null;
    this._storage = null;
    this._resetInitialization();
    this._state = {
      lastMessageId: null,
      providers: [],
      blockList: [],
      messages: [],
      ...initialState
    };
    this.onMessage = this.onMessage.bind(this);
  }

  get state() {
    return this._state;
  }

  set state(value) {
    throw new Error("Do not modify this.state directy. Instead, call this.setState(newState)");
  }

  /**
   * _resetInitialization - adds the following to the instance:
   *  .initialized {bool}            Has AS Router been initialized?
   *  .waitForInitialized {Promise}  A promise that resolves when initializion is complete
   *  ._finishInitializing {func}    A function that, when called, resolves the .waitForInitialized
   *                                 promise and sets .initialized to true.
   * @memberof _ASRouter
   */
  _resetInitialization() {
    this.initialized = false;
    this.waitForInitialized = new Promise(resolve => {
      this._finishInitializing = () => {
        this.initialized = true;
        resolve();
      };
    });
  }

  /**
   * loadMessagesFromAllProviders - Loads messages from all providers if they require updates.
   *                                Checks the .lastUpdated field on each provider to see if updates are needed
   * @memberof _ASRouter
   */
  async loadMessagesFromAllProviders() {
    const needsUpdate = this.state.providers.filter(provider => MessageLoaderUtils.shouldProviderUpdate(provider));
    // Don't do extra work if we don't need any updates
    if (needsUpdate.length) {
      let newState = {messages: [], providers: []};
      for (const provider of this.state.providers) {
        if (needsUpdate.includes(provider)) {
          const {messages, lastUpdated} = await MessageLoaderUtils.loadMessagesForProvider(provider);
          newState.providers.push({...provider, lastUpdated});
          newState.messages = [...newState.messages, ...messages];
        } else {
          // Skip updating this provider's messages if no update is required
          let messages = this.state.messages.filter(msg => msg.provider === provider.id);
          newState.providers.push(provider);
          newState.messages = [...newState.messages, ...messages];
        }
      }
      await this.setState(newState);
    }
  }

  /**
   * init - Initializes the MessageRouter.
   * It is ready when it has been connected to a RemotePageManager instance.
   *
   * @param {RemotePageManager} channel a RemotePageManager instance
   * @param {obj} storage an AS storage instance
   * @memberof _ASRouter
   */
  async init(channel, storage) {
    this.messageChannel = channel;
    this.messageChannel.addMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    await this.loadMessagesFromAllProviders();
    this._storage = storage;

    const blockList = await this._storage.get("blockList") || [];
    await this.setState({blockList});
    // sets .initialized to true and resolves .waitForInitialized promise
    this._finishInitializing();
  }

  uninit() {
    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_ALL"});
    this.messageChannel.removeMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this.messageChannel = null;
    this._resetInitialization();
  }

  setState(callbackOrObj) {
    const newState = (typeof callbackOrObj === "function") ? callbackOrObj(this.state) : callbackOrObj;
    this._state = {...this.state, ...newState};
    return new Promise(resolve => {
      this._onStateChanged(this.state);
      resolve();
    });
  }

  getMessageById(id) {
    return this.state.messages.find(message => message.id === id);
  }

  _onStateChanged(state) {
    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: state});
  }

  _getBundledMessages(originalMessage) {
    let bundledMessages = [];
    bundledMessages.push({content: originalMessage.content, id: originalMessage.id});
    for (const msg of this.state.messages) {
      if (msg.bundled && msg.template === originalMessage.template && msg.id !== originalMessage.id && !this.state.blockList.includes(msg.id)) {
        // Only copy the content - that's what the UI cares about
        bundledMessages.push({content: msg.content, id: msg.id});
      }

      // Stop once we have enough messages to fill a bundle
      if (bundledMessages.length === originalMessage.bundled) {
        break;
      }
    }

    // If we did not find enough messages to fill the bundle, do not send the bundle down
    if (bundledMessages.length < originalMessage.bundled) {
      return null;
    }

    return {bundle: bundledMessages, provider: originalMessage.provider, template: originalMessage.template};
  }

  _getUnblockedMessages() {
    let {state} = this;
    return state.messages.filter(item => !state.blockList.includes(item.id));
  }

  async sendNextMessage(target) {
    let bundledMessages;
    const msgs = this._getUnblockedMessages();
    let message = await ASRouterTargeting.findMatchingMessage(msgs);
    await this.setState({lastMessageId: message ? message.id : null});

    // If this message needs to be bundled with other messages of the same template, find them and bundle them together
    if (message && message.bundled) {
      bundledMessages = this._getBundledMessages(message);
    }
    if (message && !message.bundled) {
      // If we only need to send 1 message, send the message
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_MESSAGE", data: message});
    } else if (bundledMessages) {
      // If the message we want is bundled with other messages, send the entire bundle
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_BUNDLED_MESSAGES", data: bundledMessages});
    } else {
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_ALL"});
    }
  }

  async setMessageById(id) {
    await this.setState({lastMessageId: id});
    const newMessage = this.getMessageById(id);
    if (newMessage) {
      // If this message needs to be bundled with other messages of the same template, find them and bundle them together
      if (newMessage.bundled) {
        let bundledMessages = this._getBundledMessages(newMessage);
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_BUNDLED_MESSAGES", data: bundledMessages});
      } else {
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_MESSAGE", data: newMessage});
      }
    }
  }

  async blockById(idOrIds) {
    const idsToBlock = Array.isArray(idOrIds) ? idOrIds : [idOrIds];
    await this.setState(state => {
      const blockList = [...state.blockList, ...idsToBlock];
      this._storage.set("blockList", blockList);
      return {blockList};
    });
  }

  openLinkIn(url, target, {isPrivate = false, trusted = false, where = ""}) {
    const win = target.browser.ownerGlobal;
    const params = {
      private: isPrivate,
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})
    };
    if (trusted) {
      win.openTrustedLinkIn(url, where);
    } else {
      win.openLinkIn(url, where, params);
    }
  }

  async onMessage({data: action, target}) {
    switch (action.type) {
      case "CONNECT_UI_REQUEST":
      case "GET_NEXT_MESSAGE":
        // Wait for our initial message loading to be done before responding to any UI requests
        await this.waitForInitialized;
        // Check if any updates are needed first
        await this.loadMessagesFromAllProviders();
        await this.sendNextMessage(target);
        break;
      case ra.OPEN_PRIVATE_BROWSER_WINDOW:
        // Forcefully open about:privatebrowsing
        target.browser.ownerGlobal.OpenBrowserWindow({private: true});
        break;
      case ra.OPEN_URL:
        this.openLinkIn(action.data.button_action_params, target, {isPrivate: false, where: "tabshifted"});
        break;
      case ra.OPEN_ABOUT_PAGE:
        this.openLinkIn(`about:${action.data.button_action_params}`, target, {isPrivate: false, trusted: true, where: "tab"});
        break;
      case "BLOCK_MESSAGE_BY_ID":
        await this.blockById(action.data.id);
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE", data: {id: action.data.id}});
        break;
      case "BLOCK_BUNDLE":
        await this.blockById(action.data.bundle.map(b => b.id));
        this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_BUNDLE"});
        break;
      case "UNBLOCK_MESSAGE_BY_ID":
        await this.setState(state => {
          const blockList = [...state.blockList];
          blockList.splice(blockList.indexOf(action.data.id), 1);
          this._storage.set("blockList", blockList);
          return {blockList};
        });
        break;
      case "UNBLOCK_BUNDLE":
        await this.setState(state => {
          const blockList = [...state.blockList];
          for (let message of action.data.bundle) {
            blockList.splice(blockList.indexOf(message.id), 1);
          }
          this._storage.set("blockList", blockList);
          return {blockList};
        });
        break;
      case "OVERRIDE_MESSAGE":
        await this.setMessageById(action.data.id);
        break;
      case "ADMIN_CONNECT_STATE":
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: this.state});
        break;
    }
  }
}
this._ASRouter = _ASRouter;

/**
 * ASRouter - singleton instance of _ASRouter that controls all messages
 * in the new tab page.
 */
this.ASRouter = new _ASRouter({
  providers: [
    {id: "onboarding", type: "local", messages: OnboardingMessageProvider.getMessages()},
    {id: "snippets", type: "remote", url: SNIPPETS_ENDPOINT, updateCycleInMs: ONE_HOUR_IN_MS * 4}
  ]
});

const EXPORTED_SYMBOLS = ["_ASRouter", "ASRouter", "MessageLoaderUtils"];
