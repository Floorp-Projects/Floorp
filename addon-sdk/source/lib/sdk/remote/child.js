/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { isChildLoader } = require('./core');
if (!isChildLoader)
  throw new Error("Cannot load sdk/remote/child in a main process loader.");

const { Ci, Cc, Cu } = require('chrome');
const runtime = require('../system/runtime');
const { Class } = require('../core/heritage');
const { Namespace } = require('../core/namespace');
const { omit } = require('../util/object');
const { when } = require('../system/unload');
const { EventTarget } = require('../event/target');
const { emit } = require('../event/core');
const { Disposable } = require('../core/disposable');
const { EventParent } = require('./utils');
const { addListItem, removeListItem } = require('../util/list');

const loaderID = require('@loader/options').loaderID;

const MAIN_PROCESS = Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

const mm = Cc['@mozilla.org/childprocessmessagemanager;1'].
           getService(Ci.nsISyncMessageSender);

const ns = Namespace();

const process = {
  port: new EventTarget(),
  get id() {
    return runtime.processID;
  },
  get isRemote() {
    return runtime.processType != MAIN_PROCESS;
  }
};
exports.process = process;

function definePort(obj, name) {
  obj.port.emit = (event, ...args) => {
    let manager = ns(obj).messageManager;
    if (!manager)
      return;

    manager.sendAsyncMessage(name, { loaderID, event, args });
  };
}

function messageReceived({ data, objects }) {
  // Ignore messages from other loaders
  if (data.loaderID != loaderID)
    return;

  let keys = Object.keys(objects);
  if (keys.length) {
    // If any objects are CPOWs then ignore this message. We don't want child
    // processes interracting with CPOWs
    if (!keys.every(name => !Cu.isCrossProcessWrapper(objects[name])))
      return;

    data.args.push(objects);
  }

  emit(this.port, data.event, this, ...data.args);
}

ns(process).messageManager = mm;
definePort(process, 'sdk/remote/process/message');
let processMessageReceived = messageReceived.bind(process);
mm.addMessageListener('sdk/remote/process/message', processMessageReceived);

when(() => {
  mm.removeMessageListener('sdk/remote/process/message', processMessageReceived);
  frames = null;
});

process.port.on('sdk/remote/require', (process, uri) => {
  require(uri);
});

function listenerEquals(a, b) {
  for (let prop of ["type", "callback", "isCapturing"]) {
    if (a[prop] != b[prop])
      return false;
  }
  return true;
}

function listenerFor(type, callback, isCapturing = false) {
  return {
    type,
    callback,
    isCapturing,
    registeredCallback: undefined,
    get args() {
      return [
        this.type,
        this.registeredCallback ? this.registeredCallback : this.callback,
        this.isCapturing
      ];
    }
  };
}

function removeListenerFromArray(array, listener) {
  let index = array.findIndex(l => listenerEquals(l, listener));
  if (index < 0)
    return;
  array.splice(index, 1);
}

function getListenerFromArray(array, listener) {
  return array.find(l => listenerEquals(l, listener));
}

function arrayContainsListener(array, listener) {
  return !!getListenerFromArray(array, listener);
}

function makeFrameEventListener(frame, callback) {
  return callback.bind(frame);
}

var FRAME_ID = 0;
var tabMap = new Map();

const Frame = Class({
  implements: [ Disposable ],
  extends: EventTarget,
  setup: function(contentFrame) {
    // This ID should be unique for this loader across all processes
    ns(this).id = runtime.processID + ":" + FRAME_ID++;

    ns(this).contentFrame = contentFrame;
    ns(this).messageManager = contentFrame;
    ns(this).domListeners = [];

    tabMap.set(contentFrame.docShell, this);

    ns(this).messageReceived = messageReceived.bind(this);
    ns(this).messageManager.addMessageListener('sdk/remote/frame/message', ns(this).messageReceived);

    this.port = new EventTarget();
    definePort(this, 'sdk/remote/frame/message');

    ns(this).messageManager.sendAsyncMessage('sdk/remote/frame/attach', {
      loaderID,
      frameID: ns(this).id,
      processID: runtime.processID
    });

    frames.attachItem(this);
  },

  dispose: function() {
    emit(this, 'detach', this);

    for (let listener of ns(this).domListeners)
      ns(this).contentFrame.removeEventListener(...listener.args);

    ns(this).messageManager.removeMessageListener('sdk/remote/frame/message', ns(this).messageReceived);
    tabMap.delete(ns(this).contentFrame.docShell);
    ns(this).contentFrame = null;
  },

  get content() {
    return ns(this).contentFrame.content;
  },

  get isTab() {
    let docShell = ns(this).contentFrame.docShell;
    if (process.isRemote) {
      // We don't want to roundtrip to the main process to get this property.
      // This hack relies on the host app having defined webBrowserChrome only
      // in frames that are part of the tabs. Since only Firefox has remote
      // processes right now and does this this works.
      let tabchild = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsITabChild);
      return !!tabchild.webBrowserChrome;
    }
    else {
      // This is running in the main process so we can break out to the browser
      // And check we can find a tab for the browser element directly.
      let browser = docShell.chromeEventHandler;
      let tab = require('../tabs/utils').getTabForBrowser(browser);
      return !!tab;
    }
  },

  addEventListener: function(...args) {
    let listener = listenerFor(...args);
    if (arrayContainsListener(ns(this).domListeners, listener))
      return;

    listener.registeredCallback = makeFrameEventListener(this, listener.callback);

    ns(this).domListeners.push(listener);
    ns(this).contentFrame.addEventListener(...listener.args);
  },

  removeEventListener: function(...args) {
    let listener = getListenerFromArray(ns(this).domListeners, listenerFor(...args));
    if (!listener)
      return;

    removeListenerFromArray(ns(this).domListeners, listener);
    ns(this).contentFrame.removeEventListener(...listener.args);
  }
});

const FrameList = Class({
  implements: [ EventParent, Disposable ],
  extends: EventTarget,
  setup: function() {
    EventParent.prototype.initialize.call(this);

    this.port = new EventTarget();
    ns(this).domListeners = [];

    this.on('attach', frame => {
      for (let listener of ns(this).domListeners)
        frame.addEventListener(...listener.args);
    });
  },

  dispose: function() {
    // The only case where we get destroyed is when the loader is unloaded in
    // which case each frame will clean up its own event listeners.
    ns(this).domListeners = null;
  },

  getFrameForWindow: function(window) {
    for (let frame of this) {
      if (frame.content == window)
        return frame;
    }

    return null;
  },

  addEventListener: function(...args) {
    let listener = listenerFor(...args);
    if (arrayContainsListener(ns(this).domListeners, listener))
      return;

    ns(this).domListeners.push(listener);
    for (let frame of this)
      frame.addEventListener(...listener.args);
  },

  removeEventListener: function(...args) {
    let listener = listenerFor(...args);
    if (!arrayContainsListener(ns(this).domListeners, listener))
      return;

    removeListenerFromArray(ns(this).domListeners, listener);
    for (let frame of this)
      frame.removeEventListener(...listener.args);
  }
});
var frames = exports.frames = new FrameList();

function registerContentFrame(contentFrame) {
  let frame = new Frame(contentFrame);
}
exports.registerContentFrame = registerContentFrame;

function unregisterContentFrame(contentFrame) {
  let frame = tabMap.get(contentFrame.docShell);
  if (!frame)
    return;

  frame.destroy();
}
exports.unregisterContentFrame = unregisterContentFrame;
