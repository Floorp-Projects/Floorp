/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* vim: set ft=javascript : */

"use strict";

var Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

var EXPORTED_SYMBOLS = ["BrowserElementPromptService"];

function debug(msg) {
  // dump("BrowserElementPromptService - " + msg + "\n");
}

function BrowserElementPrompt(win, browserElementChild) {
  this._win = win;
  this._browserElementChild = browserElementChild;
}

BrowserElementPrompt.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIPrompt"]),

  alert(title, text) {
    this._browserElementChild.showModalPrompt(this._win, {
      promptType: "alert",
      title,
      message: text,
      returnValue: undefined,
    });
  },

  alertCheck(title, text, checkMsg, checkState) {
    // Treat this like a normal alert() call, ignoring the checkState.  The
    // front-end can do its own suppression of the alert() if it wants.
    this.alert(title, text);
  },

  confirm(title, text) {
    return this._browserElementChild.showModalPrompt(this._win, {
      promptType: "confirm",
      title,
      message: text,
      returnValue: undefined,
    });
  },

  confirmCheck(title, text, checkMsg, checkState) {
    return this.confirm(title, text);
  },

  // Each button is described by an object with the following schema
  // {
  //   string messageType,  // 'builtin' or 'custom'
  //   string message, // 'ok', 'cancel', 'yes', 'no', 'save', 'dontsave',
  //                   // 'revert' or a string from caller if messageType was 'custom'.
  // }
  //
  // Expected result from embedder:
  // {
  //   int button, // Index of the button that user pressed.
  //   boolean checked, // True if the check box is checked.
  // }
  confirmEx(
    title,
    text,
    buttonFlags,
    button0Title,
    button1Title,
    button2Title,
    checkMsg,
    checkState
  ) {
    let buttonProperties = this._buildConfirmExButtonProperties(
      buttonFlags,
      button0Title,
      button1Title,
      button2Title
    );
    let defaultReturnValue = { selectedButton: buttonProperties.defaultButton };
    if (checkMsg) {
      defaultReturnValue.checked = checkState.value;
    }
    let ret = this._browserElementChild.showModalPrompt(this._win, {
      promptType: "custom-prompt",
      title,
      message: text,
      defaultButton: buttonProperties.defaultButton,
      buttons: buttonProperties.buttons,
      showCheckbox: !!checkMsg,
      checkboxMessage: checkMsg,
      checkboxCheckedByDefault: !!checkState.value,
      returnValue: defaultReturnValue,
    });
    if (checkMsg) {
      checkState.value = ret.checked;
    }
    return buttonProperties.indexToButtonNumberMap[ret.selectedButton];
  },

  prompt(title, text, value, checkMsg, checkState) {
    let rv = this._browserElementChild.showModalPrompt(this._win, {
      promptType: "prompt",
      title,
      message: text,
      initialValue: value.value,
      returnValue: null,
    });

    value.value = rv;

    // nsIPrompt::Prompt returns true if the user pressed "OK" at the prompt,
    // and false if the user pressed "Cancel".
    //
    // BrowserElementChild returns null for "Cancel" and returns the string the
    // user entered otherwise.
    return rv !== null;
  },

  promptUsernameAndPassword(title, text, username, password) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  promptPassword(title, text, password) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  select(title, text, aSelectList, aOutSelection) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  _buildConfirmExButtonProperties(
    buttonFlags,
    button0Title,
    button1Title,
    button2Title
  ) {
    let r = {
      defaultButton: -1,
      buttons: [],
      // This map is for translating array index to the button number that
      // is recognized by Gecko. This shouldn't be exposed to embedder.
      indexToButtonNumberMap: [],
    };

    let defaultButton = 0; // Default to Button 0.
    if (buttonFlags & Ci.nsIPrompt.BUTTON_POS_1_DEFAULT) {
      defaultButton = 1;
    } else if (buttonFlags & Ci.nsIPrompt.BUTTON_POS_2_DEFAULT) {
      defaultButton = 2;
    }

    // Properties of each button.
    let buttonPositions = [
      Ci.nsIPrompt.BUTTON_POS_0,
      Ci.nsIPrompt.BUTTON_POS_1,
      Ci.nsIPrompt.BUTTON_POS_2,
    ];

    function buildButton(buttonTitle, buttonNumber) {
      let ret = {};
      let buttonPosition = buttonPositions[buttonNumber];
      let mask = 0xff * buttonPosition; // 8 bit mask
      let titleType = (buttonFlags & mask) / buttonPosition;

      ret.messageType = "builtin";
      switch (titleType) {
        case Ci.nsIPrompt.BUTTON_TITLE_OK:
          ret.message = "ok";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_CANCEL:
          ret.message = "cancel";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_YES:
          ret.message = "yes";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_NO:
          ret.message = "no";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_SAVE:
          ret.message = "save";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE:
          ret.message = "dontsave";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_REVERT:
          ret.message = "revert";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_IS_STRING:
          ret.message = buttonTitle;
          ret.messageType = "custom";
          break;
        default:
          // This button is not shown.
          return;
      }

      // If this is the default button, set r.defaultButton to
      // the index of this button in the array. This value is going to be
      // exposed to the embedder.
      if (defaultButton === buttonNumber) {
        r.defaultButton = r.buttons.length;
      }
      r.buttons.push(ret);
      r.indexToButtonNumberMap.push(buttonNumber);
    }

    buildButton(button0Title, 0);
    buildButton(button1Title, 1);
    buildButton(button2Title, 2);

    // If defaultButton is still -1 here, it means the default button won't
    // be shown.
    if (r.defaultButton === -1) {
      throw new Components.Exception(
        "Default button won't be shown",
        Cr.NS_ERROR_FAILURE
      );
    }

    return r;
  },
};

function BrowserElementAuthPrompt() {}

BrowserElementAuthPrompt.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),

  promptAuth: function promptAuth(channel, level, authInfo) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },

  asyncPromptAuth: function asyncPromptAuth(
    channel,
    callback,
    context,
    level,
    authInfo
  ) {
    debug("asyncPromptAuth");

    // The cases that we don't support now.
    if (
      authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY &&
      authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD
    ) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    let frame = this._getFrameFromChannel(channel);
    if (!frame) {
      debug("Cannot get frame, asyncPromptAuth fail");
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    let browserElementParent = BrowserElementPromptService.getBrowserElementParentForFrame(
      frame
    );

    if (!browserElementParent) {
      debug("Failed to load browser element parent.");
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    let consumer = {
      QueryInterface: ChromeUtils.generateQI(["nsICancelable"]),
      callback,
      context,
      cancel() {
        this.callback.onAuthCancelled(this.context, false);
        this.callback = null;
        this.context = null;
      },
    };

    let [hostname, httpRealm] = this._getAuthTarget(channel, authInfo);
    let hashKey = level + "|" + hostname + "|" + httpRealm;
    let asyncPrompt = this._asyncPrompts[hashKey];
    if (asyncPrompt) {
      asyncPrompt.consumers.push(consumer);
      return consumer;
    }

    asyncPrompt = {
      consumers: [consumer],
      channel,
      authInfo,
      level,
      inProgress: false,
      browserElementParent,
    };

    this._asyncPrompts[hashKey] = asyncPrompt;
    this._doAsyncPrompt();
    return consumer;
  },

  // Utilities for nsIAuthPrompt2 ----------------

  _asyncPrompts: {},
  _asyncPromptInProgress: new WeakMap(),
  _doAsyncPrompt() {
    // Find the key of a prompt whose browser element parent does not have
    // async prompt in progress.
    let hashKey = null;
    for (let key in this._asyncPrompts) {
      let prompt = this._asyncPrompts[key];
      if (!this._asyncPromptInProgress.get(prompt.browserElementParent)) {
        hashKey = key;
        break;
      }
    }

    // Didn't find an available prompt, so just return.
    if (!hashKey) {
      return;
    }

    let prompt = this._asyncPrompts[hashKey];

    this._asyncPromptInProgress.set(prompt.browserElementParent, true);
    prompt.inProgress = true;

    let self = this;
    let callback = function(ok, username, password) {
      debug(
        "Async auth callback is called, ok = " + ok + ", username = " + username
      );

      // Here we got the username and password provided by embedder, or
      // ok = false if the prompt was cancelled by embedder.
      delete self._asyncPrompts[hashKey];
      prompt.inProgress = false;
      self._asyncPromptInProgress.delete(prompt.browserElementParent);

      // Fill authentication information with username and password provided
      // by user.
      let flags = prompt.authInfo.flags;
      if (username) {
        if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
          // Domain is separated from username by a backslash
          let idx = username.indexOf("\\");
          if (idx == -1) {
            prompt.authInfo.username = username;
          } else {
            prompt.authInfo.domain = username.substring(0, idx);
            prompt.authInfo.username = username.substring(idx + 1);
          }
        } else {
          prompt.authInfo.username = username;
        }
      }

      if (password) {
        prompt.authInfo.password = password;
      }

      for (let consumer of prompt.consumers) {
        if (!consumer.callback) {
          // Not having a callback means that consumer didn't provide it
          // or canceled the notification.
          continue;
        }

        try {
          if (ok) {
            debug("Ok, calling onAuthAvailable to finish auth");
            consumer.callback.onAuthAvailable(
              consumer.context,
              prompt.authInfo
            );
          } else {
            debug("Cancelled, calling onAuthCancelled to finish auth.");
            consumer.callback.onAuthCancelled(consumer.context, true);
          }
        } catch (e) {
          /* Throw away exceptions caused by callback */
        }
      }

      // Process the next prompt, if one is pending.
      self._doAsyncPrompt();
    };

    let runnable = {
      run() {
        // Call promptAuth of browserElementParent, to show the prompt.
        prompt.browserElementParent.promptAuth(
          self._createAuthDetail(prompt.channel, prompt.authInfo),
          callback
        );
      },
    };

    Services.tm.dispatchToMainThread(runnable);
  },

  _getFrameFromChannel(channel) {
    let loadContext = channel.notificationCallbacks.getInterface(
      Ci.nsILoadContext
    );
    return loadContext.topFrameElement;
  },

  _createAuthDetail(channel, authInfo) {
    let [hostname, httpRealm] = this._getAuthTarget(channel, authInfo);
    return {
      host: hostname,
      path: channel.URI.pathQueryRef,
      realm: httpRealm,
      username: authInfo.username,
      isProxy: !!(authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY),
      isOnlyPassword: !!(authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD),
    };
  },

  // The code is taken from nsLoginManagerPrompter.js, with slight
  // modification for parameter name consistency here.
  _getAuthTarget(channel, authInfo) {
    let hostname, realm;

    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      if (!(channel instanceof Ci.nsIProxiedChannel)) {
        throw new Error("proxy auth needs nsIProxiedChannel");
      }

      let info = channel.proxyInfo;
      if (!info) {
        throw new Error("proxy auth needs nsIProxyInfo");
      }

      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
        Ci.nsIIDNService
      );
      hostname =
        "moz-proxy://" +
        idnService.convertUTF8toACE(info.host) +
        ":" +
        info.port;
      realm = authInfo.realm;
      if (!realm) {
        realm = hostname;
      }

      return [hostname, realm];
    }

    hostname = this._getFormattedHostname(channel.URI);

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    realm = authInfo.realm;
    if (!realm) {
      realm = hostname;
    }

    return [hostname, realm];
  },

  /**
   * Strip out things like userPass and path for display.
   */
  _getFormattedHostname(uri) {
    return uri.scheme + "://" + uri.hostPort;
  },
};

function AuthPromptWrapper(oldImpl, browserElementImpl) {
  this._oldImpl = oldImpl;
  this._browserElementImpl = browserElementImpl;
}

AuthPromptWrapper.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),
  promptAuth(channel, level, authInfo) {
    if (this._canGetParentElement(channel)) {
      return this._browserElementImpl.promptAuth(channel, level, authInfo);
    }
    return this._oldImpl.promptAuth(channel, level, authInfo);
  },

  asyncPromptAuth(channel, callback, context, level, authInfo) {
    if (this._canGetParentElement(channel)) {
      return this._browserElementImpl.asyncPromptAuth(
        channel,
        callback,
        context,
        level,
        authInfo
      );
    }
    return this._oldImpl.asyncPromptAuth(
      channel,
      callback,
      context,
      level,
      authInfo
    );
  },

  _canGetParentElement(channel) {
    try {
      let context = channel.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
      let frame = context.topFrameElement;
      if (!frame) {
        return false;
      }

      if (!BrowserElementPromptService.getBrowserElementParentForFrame(frame)) {
        return false;
      }

      return true;
    } catch (e) {
      return false;
    }
  },
};

function BrowserElementPromptFactory(toWrap) {
  this._wrapped = toWrap;
}

BrowserElementPromptFactory.prototype = {
  classID: Components.ID("{24f3d0cf-e417-4b85-9017-c9ecf8bb1299}"),
  QueryInterface: ChromeUtils.generateQI(["nsIPromptFactory"]),

  _mayUseNativePrompt() {
    try {
      return Services.prefs.getBoolPref("browser.prompt.allowNative");
    } catch (e) {
      // This properity is default to true.
      return true;
    }
  },

  _getNativePromptIfAllowed(win, iid, err) {
    if (this._mayUseNativePrompt()) {
      return this._wrapped.getPrompt(win, iid);
    }

    // Not allowed, throw an exception.
    throw err;
  },

  getPrompt(win, iid) {
    // It is possible for some object to get a prompt without passing
    // valid reference of window, like nsNSSComponent. In such case, we
    // should just fall back to the native prompt service
    if (!win) {
      return this._getNativePromptIfAllowed(win, iid, Cr.NS_ERROR_INVALID_ARG);
    }

    if (
      iid.number != Ci.nsIPrompt.number &&
      iid.number != Ci.nsIAuthPrompt2.number
    ) {
      debug(
        "We don't recognize the requested IID (" +
          iid +
          ", " +
          "allowed IID: " +
          "nsIPrompt=" +
          Ci.nsIPrompt +
          ", " +
          "nsIAuthPrompt2=" +
          Ci.nsIAuthPrompt2 +
          ")"
      );
      return this._getNativePromptIfAllowed(win, iid, Cr.NS_ERROR_INVALID_ARG);
    }

    // Try to find a BrowserElementChild for the window.
    let browserElementChild = BrowserElementPromptService.getBrowserElementChildForWindow(
      win
    );

    if (iid.number === Ci.nsIAuthPrompt2.number) {
      debug("Caller requests an instance of nsIAuthPrompt2.");

      if (browserElementChild) {
        // If we are able to get a BrowserElementChild, it means that
        // the auth prompt is for a mozbrowser. Therefore we don't need to
        // fall back.
        return new BrowserElementAuthPrompt().QueryInterface(iid);
      }

      // Because nsIAuthPrompt2 is called in parent process. If caller
      // wants nsIAuthPrompt2 and we cannot get BrowserElementchild,
      // it doesn't mean that we should fallback. It is possible that we can
      // get the BrowserElementParent from nsIChannel that passed to
      // functions of nsIAuthPrompt2.
      if (this._mayUseNativePrompt()) {
        return new AuthPromptWrapper(
          this._wrapped.getPrompt(win, iid),
          new BrowserElementAuthPrompt().QueryInterface(iid)
        ).QueryInterface(iid);
      }
      // Falling back is not allowed, so we don't need wrap the
      // BrowserElementPrompt.
      return new BrowserElementAuthPrompt().QueryInterface(iid);
    }

    if (!browserElementChild) {
      debug(
        "We can't find a browserElementChild for " + win + ", " + win.location
      );
      return this._getNativePromptIfAllowed(win, iid, Cr.NS_ERROR_FAILURE);
    }

    debug("Returning wrapped getPrompt for " + win);
    return new BrowserElementPrompt(win, browserElementChild).QueryInterface(
      iid
    );
  },
};

var BrowserElementPromptService = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  _initialized: false,

  _init() {
    if (this._initialized) {
      return;
    }

    this._initialized = true;
    this._browserElementParentMap = new WeakMap();

    Services.obs.addObserver(
      this,
      "outer-window-destroyed",
      /* ownsWeak = */ true
    );

    // Wrap the existing @mozilla.org/prompter;1 implementation.
    var contractID = "@mozilla.org/prompter;1";
    var oldCID = Cm.contractIDToCID(contractID);
    var newCID = BrowserElementPromptFactory.prototype.classID;
    var oldFactory = Cm.getClassObject(Cc[contractID], Ci.nsIFactory);

    if (oldCID == newCID) {
      debug("WARNING: Wrapped prompt factory is already installed!");
      return;
    }

    var oldInstance = oldFactory.createInstance(null, Ci.nsIPromptFactory);
    var newInstance = new BrowserElementPromptFactory(oldInstance);

    var newFactory = {
      createInstance(iid) {
        return newInstance.QueryInterface(iid);
      },
    };
    Cm.registerFactory(
      newCID,
      "BrowserElementPromptService's prompter;1 wrapper",
      contractID,
      newFactory
    );

    debug("Done installing new prompt factory.");
  },

  _getOuterWindowID(win) {
    return win.docShell.outerWindowID;
  },

  _browserElementChildMap: {},
  mapWindowToBrowserElementChild(win, browserElementChild) {
    this._browserElementChildMap[
      this._getOuterWindowID(win)
    ] = browserElementChild;
  },
  unmapWindowToBrowserElementChild(win) {
    delete this._browserElementChildMap[this._getOuterWindowID(win)];
  },

  getBrowserElementChildForWindow(win) {
    // We only have a mapping for <iframe mozbrowser>s, not their inner
    // <iframes>, so we look up win.top below.  window.top (when called from
    // script) respects <iframe mozbrowser> boundaries.
    return this._browserElementChildMap[this._getOuterWindowID(win.top)];
  },

  mapFrameToBrowserElementParent(frame, browserElementParent) {
    this._browserElementParentMap.set(frame, browserElementParent);
  },

  getBrowserElementParentForFrame(frame) {
    return this._browserElementParentMap.get(frame);
  },

  _observeOuterWindowDestroyed(outerWindowID) {
    let id = outerWindowID.QueryInterface(Ci.nsISupportsPRUint64).data;
    debug("observeOuterWindowDestroyed " + id);
    delete this._browserElementChildMap[outerWindowID.data];
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "outer-window-destroyed":
        this._observeOuterWindowDestroyed(subject);
        break;
      default:
        debug("Observed unexpected topic " + topic);
    }
  },
};

BrowserElementPromptService._init();
