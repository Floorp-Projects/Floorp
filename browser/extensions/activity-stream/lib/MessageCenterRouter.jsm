const INCOMING_MESSAGE_NAME = "MessageCenter:child-to-parent";
const OUTGOING_MESSAGE_NAME = "MessageCenter:parent-to-child";

const FAKE_MESSAGES = [
  {
    id: "ONBOARDING_1",
    template: "simple_snippet",
    content: {
      title: "Find it faster",
      body: "Access all of your favorite search engines with a click. Search the whole Web or just one website from the search box.",
      button: {
        label: "Learn More",
        action: "OPEN_LINK",
        params: {url: "https://mozilla.org"}
      }
    }
  },
  {
    id: "ONBOARDING_2",
    template: "simple_snippet",
    content: {
      title: "Make Firefox your go-to-browser",
      body: "It doesn't take much to get the most from Firefox. Just set Firefox as your default browser and put control, customization, and protection on autopilot."
    }
  },
  {
    id: "ONBOARDING_3",
    template: "simple_snippet",
    content: {
      title: "Did you know?",
      body: "All human beings are born free and equal in dignity and rights. They are endowed with reason and conscience and should act towards one another in a spirit of brotherhood."
    }
  }
];

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
 * @class _MessageCenterRouter - Keeps track of all messages, UI surfaces, and
 * handles blocking, rotation, etc. Inspecting MessageCenter.state will
 * tell you what the current displayed message is in all UI surfaces.
 *
 * Note: This is written as a constructor rather than just a plain object
 * so that it can be more easily unit tested.
 */
class _MessageCenterRouter {
  constructor(initialState = {}) {
    this.initialized = false;
    this.messageChannel = null;
    this._state = Object.assign({
      currentId: null,
      blockList: {},
      messages: {}
    }, initialState);
    this.onMessage = this.onMessage.bind(this);
  }

  get state() {
    return this._state;
  }

  set state(value) {
    throw new Error("Do not modify this.state directy. Instead, call this.setState(newState)");
  }

  /**
   * init - Initializes the MessageRouter.
   * It is ready when it has been connected to a RemotePageManager instance.
   *
   * @param {RemotePageManager} channel a RemotePageManager instance
   * @memberof _MessageCenterRouter
   */
  init(channel) {
    this.messageChannel = channel;
    this.messageChannel.addMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this.initialized = true;
  }

  uninit() {
    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
    this.messageChannel.removeMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this.messageChannel = null;
    this.initialized = false;
  }

  setState(callbackOrObj) {
    const newState = (typeof callbackOrObj === "function") ? callbackOrObj(this.state) : callbackOrObj;
    this._state = Object.assign({}, this.state, newState);
    return new Promise(resolve => {
      this._onStateChanged(this.state);
      resolve();
    });
  }

  _onStateChanged(state) {
    this.messageChannel.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: state});
  }

  async sendNextMessage(target, id) {
    let message;
    await this.setState(state => {
      message = getRandomItemFromArray(state.messages.filter(item => item.id !== state.currentId && !state.blockList[item.id]));
      return {currentId: message ? message.id : null};
    });
    if (message) {
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "SET_MESSAGE", data: message});
    } else {
      target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "CLEAR_MESSAGE"});
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
        await this.sendNextMessage(target);
        break;
      case "BLOCK_MESSAGE_BY_ID":
        await this.setState(state => {
          const newState = Object.assign({}, state.blockList);
          newState[action.data.id] = true;
          return {blockList: newState};
        });
        await this.clearMessage(target, action.data.id);
        break;
      case "UNBLOCK_MESSAGE_BY_ID":
        await this.setState(state => {
          const newState = Object.assign({}, state.blockList);
          delete newState[action.data.id];
          return {blockList: newState};
        });
        break;
      case "ADMIN_CONNECT_STATE":
        target.sendAsyncMessage(OUTGOING_MESSAGE_NAME, {type: "ADMIN_SET_STATE", data: this.state});
        break;
    }
  }
}
this._MessageCenterRouter = _MessageCenterRouter;

/**
 * MessageCenterRouter - singleton instance of _MessageCenterRouter that controls all messages
 * in the new tab page.
 */
this.MessageCenterRouter = new _MessageCenterRouter({messages: FAKE_MESSAGES});

const EXPORTED_SYMBOLS = ["_MessageCenterRouter", "MessageCenterRouter"];
