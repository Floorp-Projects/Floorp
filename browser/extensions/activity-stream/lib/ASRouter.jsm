ChromeUtils.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["fetch"]);

const INCOMING_MESSAGE_NAME = "ASRouter:child-to-parent";
const OUTGOING_MESSAGE_NAME = "ASRouter:parent-to-child";
const ONE_HOUR_IN_MS = 60 * 60 * 1000;
// This is a temporary endpoint until we have something for snippets
const SNIPPETS_ENDPOINT = "https://activity-stream-icons.services.mozilla.com/v1/messages.json.br";

const LOCAL_TEST_MESSAGES = [
  {
    id: "LOCAL_TEST_THEMES",
    template: "simple_snippet",
    content: {
      text: "Your browser is ready for a makeover. Don't worry, you've got tons of options.",
      button_label: "Check them out here",
      button_url: "https://addons.mozilla.org/en-US/firefox/themes"
    }
  }
];

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
        remoteMessages = (await (await fetch(provider.url)).json())
          .messages
          .map(msg => (Object.assign({}, msg, {provider_url: provider.url})));
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
        .map(msg => Object.assign({}, msg, {provider: provider.id}));
    const lastUpdated = Date.now();
    return {messages, lastUpdated};
  }
};

this.MessageLoaderUtils = MessageLoaderUtils;

/**
 * getRandomItemFromArray
 *
 * @param {Array} arr An array of items
 * @returns one of the items in the array
 */
function getRandomItemFromArray(arr) {
  const index = Math.floor(Math.random() * arr.length);
  return arr[index];
}

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
    this._state = Object.assign(
      {
        currentId: null,
        providers: [],
        blockList: [],
        messages: []
      },
      initialState
    );
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
          newState.providers.push((Object.assign({}, provider, {lastUpdated})));
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
    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
    this.messageChannel.removeMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this.messageChannel = null;
    this._resetInitialization();
  }

  setState(callbackOrObj) {
    const newState = (typeof callbackOrObj === "function") ? callbackOrObj(this.state) : callbackOrObj;
    this._state = Object.assign({}, this.state, newState);
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

  async sendNextMessage(target, id) {
    let message;

    await this.setState(state => {
      message = getRandomItemFromArray(state.messages.filter(item => item.id !== state.currentId && !state.blockList.includes(item.id)));
      return {currentId: message ? message.id : null};
    });
    if (message) {
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_MESSAGE", data: message});
    } else {
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
    }
  }

  async setMessageById(id) {
    await this.setState({currentId: id});
    const newMessage = this.getMessageById(id);
    if (newMessage) {
      this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_MESSAGE", data: newMessage});
    }
  }

  async clearMessage(target, id) {
    if (this.state.currentId === id) {
      await this.setState({currentId: null});
    }
    target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
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
      case "BLOCK_MESSAGE_BY_ID":
        await this.setState(state => {
          const blockList = [...state.blockList];
          blockList.push(action.data.id);
          this._storage.set("blockList", blockList);

          return {blockList};
        });
        await this.clearMessage(target, action.data.id);
        break;
      case "UNBLOCK_MESSAGE_BY_ID":
        await this.setState(state => {
          const blockList = [...state.blockList];
          blockList.splice(blockList.indexOf(action.data.id), 1);
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
    {id: "onboarding", type: "local", messages: LOCAL_TEST_MESSAGES},
    {id: "snippets", type: "remote", url: SNIPPETS_ENDPOINT, updateCycleInMs: ONE_HOUR_IN_MS * 4}
  ]
});

const EXPORTED_SYMBOLS = ["_ASRouter", "ASRouter", "MessageLoaderUtils"];
