const INCOMING_MESSAGE_NAME = "ASRouter:child-to-parent";
const OUTGOING_MESSAGE_NAME = "ASRouter:parent-to-child";

const FAKE_MESSAGES = [
  {
    id: "SUNRISE",
    template: "simple_snippet",
    content: {
      icon: "data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHhtbG5zOnhsaW5rPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hsaW5rIiB3aWR0aD0iNDIiIGhlaWdodD0iNDIiIHZpZXdCb3g9IjAgMCA0MiA0MiI+CiAgPG1ldGFkYXRhPjw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+Cjx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuNi1jMTQyIDc5LjE2MDkyNCwgMjAxNy8wNy8xMy0wMTowNjozOSAgICAgICAgIj4KICAgPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICAgICAgPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIvPgogICA8L3JkZjpSREY+CjwveDp4bXBtZXRhPgogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgCjw/eHBhY2tldCBlbmQ9InciPz48L21ldGFkYXRhPgo8aW1hZ2UgaWQ9IkxheWVyXzAiIGRhdGEtbmFtZT0iTGF5ZXIgMCIgeD0iMSIgeT0iMSIgd2lkdGg9IjQwIiBoZWlnaHQ9IjQwIiB4bGluazpocmVmPSJkYXRhOmltZy9wbmc7YmFzZTY0LGlWQk9SdzBLR2dvQUFBQU5TVWhFVWdBQUFDZ0FBQUFvQ0FZQUFBQ00vcmh0QUFBQUJHZEJUVUVBQUxHUEMveGhCUUFBQUNCalNGSk5BQUI2SmdBQWdJUUFBUG9BQUFDQTZBQUFkVEFBQU9wZ0FBQTZtQUFBRjNDY3VsRThBQUFBQm1KTFIwUUFBQUFBQUFENVE3dC9BQUFBQ1hCSVdYTUFBQXNTQUFBTEVnSFMzWDc4QUFBQUIzUkpUVVVINGdRS0R3RWZraW90QkFBQUJvNUpSRUZVV01POW1VOXNITlVad0g5djVzM3NuL0Y2dlhac0o0N3Q0SVJhRUhCRFM5UlNJQWdKeEtIcW9ZamN5cUczU2xVdnZYSklSQTRjMjNOdlBSQ3ByVUxoVUhFQ2laSlFOWWlxc2FLUU5zZ1N3U1MyeVI5N0U2OTNkMmJlZXh5KzhYclhKcmJYc2Zra1M1NTViNzczZTkvZk43UEsvZTA1ZGlnKzhEVHdGREFKN0FmSzJWZ1ZtQWV1QVplQS93Qm1KNHZvSGNMOUJQZzU4QUx3QkREMGdIbmZBRmVBajRIM2dVKzdYVWgxYWNGZkFiOEJUclR1T0VCdDhrVG4rSG5nVDhEWjdTN29iWFBlczVueXQxdHdTa0Zpb042RVJnTE9yWUVvQmFtQmVnek5HS3lUZS9MczI1bXVaM2NMOEJUd0NmQjh4OTFtQXIwRk9QNG9qQTNJZFRPVnNWb0R0QTlQanNQa1FZRTN0djNwNXpPZHA3WmFmS3NZUEFlOHR1R3VkYkxvNUVFNE5BZ2ovYkN2RjY1K0xYQUhCMlJzb0FkUWNtLzJOa1E1Y2ZtYXZBbjhFRGk1RThBUGdKZFFHVkJxd1ZQZ2VlTFNrUW9NbE9ET2ZYSGY2RDZJOG5CdlJRQ1ZndW9LNUhNd1BnanppNkpES2JDWk5iVlBab0FQZ0plN2NmRzdMYmpVaXNKaUNJRVBTU0pQSFJxU3hSeGl6Vm9UZXZJd05paXgyVWhrUEk2aHZ5UldYbW1LOWx3QW9aWTRGWGtwVzNOYkZqd0YvTExsU29ESHhtQzREeHF4TE9KNzBCZkpkU3N4RUxERXJGMkRiQ0ExOE9nQnNYZ3VoSnlHUU1QL2I4RE51d0lyYTU0Q3pyVERyQzh6VHdIL2JWMGxLWlFLOE5OSjBGb0MzZmRrMVViQ3RzVTVjV2NZaURlTWhXSU9adVpnK2tzb2hPMnpmNFFVOSs5MDhaODdycFFIY1NwV00xYUFHM0YzY0NDdU5sWktVak1SaThZcHhNbHErWGtnUXp2Z3E4Q3hqcW1lZ3RoSStmQTJxOFk3RVNlNjNZYUJZeG5MQnNBMzFuYWN1YVVScnlYRlhvaTEyUm9iMm5TTFpUVkpwb0RqQXFmRURkYkJVQm5HOWtHcEtDN1pUVWtNVE95WFpMbDVGNVlia3QyK0FzZnhqT215QmdMYUMyV1NRcmtvV2JldkYzeGZkbW5kNWoyM1cwbU5aTy9qbzFJbloyL0Q5Vy9BdU5Wd09nbjh6OHRJWCt6WTJaSDlNREc4MW12ZExzT3RlaW8xVWo5RERWUGpZcEJtS3dGZkJLWTg0Qm5hazBNQmQrOUxvLzgrWkRWSnFpdmladDBLK0dQQU14bzR3dHBCVTJyVmw3ZGdvQmYyVjZTUHFsMHluMUpTUjMwbC95c2w3dlE5dVBJVkxOYWtZNG1VZ1NNYUdPNVE0bWUxNytyWDBsdHpRWHRMNmg2b0VFZ2llRWpiWElsWldZNXBHa3RxTFRZTUtDNHNVcHE5QmVXOHJPK3l1ZFlONnc3cmdjUmJUc1Bpc3JoNmZMQTdRT2NrcGtwNWlGT3V6MWU1TWwvbDR0d1NzMHQxNXBjYnpOZWExQk5EMDFnTWlyS3pqT1kwQi9JQmgvT2FuL1dFUEYzS1V3Nzg4c1plckpSMGlvbGhPYUhFMjRSYmJXZDlSVmh1OE42L1ovanI1emM1LzlVZDV1NDNzTmttZGVDVDF6NitVbmhLUW41UkthN0doblNwSVhNOHhWUVVNQnBxTktocUs4YWN3eVVwOVNqRVB6eE1Ea1JEUHBkbHNRS2NsQ0pqQldvVnJyY0lIdno5NGd4L3VQQUZuMXkvRFVBbHlqSFNrOTh5alBzQWd1eHdCSHhSVDVtdUpWVzlqRjI0WVMzdjI1VFBuV0hPR21wR282ZXYwbU9oRW1yNkFrMS9HSEMwSjJLOFdPRHhuaUpSTVM4V1N3MFVBeFlXcXZ6dUg1YzROejBMZ2M5b0pjSlhhd2VpYllkdDlsZlJIaFZZMEw5SWFqTnoyT3FzYzJVUGlKUkNOMk5jclU3cUhMRjF4TTdpSzBXZ0ZBWGY1M0F4ejJRcDRrUi9tVmZHaHJoMmJaN1gvM0tSRy9mcUhCanNJZlE5ckhOZHc2MlRLakNqUnM1Ty9iaUErbU1CWHRoS253T3NjOXhMRGJYVUVEdkxaS25JMHAwR2k3Y2JETVFlY1MwRkQ3emdvUnY0eDhEdmRRVjFHZmpJeVR2dWx1YjNsYUlTYUNxQlJnRkx6WlN3UDhmRWdTTHBTb3ErbTlDODFjVGNTMUUrcUoyRGZnUmM5b0FFZVRucVdoelFvMzFDQzJuZG9MUkgvbUNCMHRFUzBROGl2THlQYVJpYzJWR3JQQWNrcTl1N0RIejJVQTRCbkhIWWhrR2h5SThVS0UzMVVoZ3ZBZzdic04ybytpeGo2ampwdmZXd2dDMVE2ekFyQnVVcG9vbUkwaE5sZEYrQXFXL2JtaTJXZHNCM2dlbmRna1NCU3gybWJ0QWxUZWxvTDRWSGlqanJzRTI3R2VRMGJXOTQ2eVA0MTdzRzJDYTJZY0U0aW85RWxJNzI0dVU5YlAyQkhhcURZVDNnSmVEMHJoTXFjYnV0RzhKS1NPK1RaWFIvaUtrYjZBek4wN1M5MFFINFE2OE5yMWYzVCtUMTg3RzlzS1pMSEY3T0l6ZVl3MWxMc3BTZ2xFTDU2ajNndCt2blA2aEl2UXA4dUJlQUtMQk5pek9PNkVpSjZIQ0VVdXBEek5xYjNIWUFRYjZWdkxOWGtDNlZaQ2tjS3I0VDlJY3YyL2k3eTlCV1pmNGtleEdUR1NUT25YYkduZHlNWWp0OTZBendISEJoRi9FdVpEclB0TTVYRHdFSThDL2s2K2pyeU5mUm5jcjVUTWVKVE9lVzB1MUg5TFBaMy9mMkVYMm5YL2svUlg1YTJQT2ZJYjRGR2NxUFIwMkd5OU1BQUFBQVNVVk9SSzVDWUlJPSIvPgo8L3N2Zz4K",
      text: "Profitez du temps qui passe et de la nature qui nous entoure. Si vous le pouvez, observez le lever du soleil le matin et prenez un grand bol d'air frais."
    }
  },
  {
    id: "ONBOARDING_1",
    template: "simple_snippet",
    content: {
      title: "Find it faster",
      text: "Access all of your favorite search engines with a click. Search the whole Web or just one website from the search box.",
      button_label: "Learn More",
      button_url: "https://mozilla.org"
    }
  },
  {
    id: "ONBOARDING_2",
    template: "simple_snippet",
    content: {
      title: "Make Firefox your go-to-browser",
      text: "It doesn't take much to get the most from Firefox. Just set Firefox as your default browser and put control, customization, and protection on autopilot."
    }
  },
  {
    id: "ONBOARDING_3",
    template: "simple_snippet",
    content: {
      title: "Did you know?",
      text: "All human beings are born free and equal in dignity and rights. They are endowed with reason and conscience and should act towards one another in a spirit of brotherhood."
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
    this._state = Object.assign({
      currentId: null,
      blockList: [],
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
   * @memberof _ASRouter
   */
  async init(channel, storage) {
    this.messageChannel = channel;
    this.messageChannel.addMessageListener(INCOMING_MESSAGE_NAME, this.onMessage);
    this.initialized = true;
    this._storage = storage;

    const blockList = await this._storage.get("blockList");
    this.setState({blockList});
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
      message = getRandomItemFromArray(state.messages.filter(item => item.id !== state.currentId && !state.blockList.includes(item.id)));
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
        this.setState(state => {
          const blockList = [...state.blockList];
          blockList.push(action.data.id);
          this._storage.set("blockList", blockList);

          return {blockList};
        });
        await this.clearMessage(target, action.data.id);
        break;
      case "UNBLOCK_MESSAGE_BY_ID":
        this.setState(state => {
          const blockList = [...state.blockList];
          blockList.splice(blockList.indexOf(action.data.id), 1);
          this._storage.set("blockList", blockList);

          return {blockList};
        });
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
this.ASRouter = new _ASRouter({messages: FAKE_MESSAGES});

const EXPORTED_SYMBOLS = ["_ASRouter", "ASRouter"];
