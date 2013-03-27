/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

this.EXPORTED_SYMBOLS = ['SignInToWebsiteUX'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/identity/IdentityUtils.jsm');

const kIdentityScreen = 'https://picl.personatest.org/sign_in#NATIVE';
const kIdentityFrame = 'https://picl.personatest.org/communication_iframe';
const kIdentityShim = 'chrome://browser/content/browser-identity.js';

const PANEL_MIN_HEIGHT = 440;
const PANEL_MIN_WIDTH = 300;

XPCOMUtils.defineLazyModuleGetter(this, 'IdentityService',
                                  'resource://gre/modules/identity/MinimalIdentity.jsm');

XPCOMUtils.defineLazyGetter(this, "logger", function() {
  Cu.import('resource://gre/modules/identity/LogUtils.jsm');
  return getLogger("Identity", "toolkit.identity.debug");
});

/**
 * Ripped off from the resize watcher in base/content/browser-social.js
 */

function sizePanelToContent(iframe) {
  // FIXME: bug 764787: Maybe we can use nsIDOMWindowUtils.getRootBounds() here?
  let doc = iframe.contentDocument;
  if (!doc || !doc.body) {
    return;
  }
  let body = doc.body;

  // offsetHeight/Width don't include margins, so account for that.
  let cs = doc.defaultView.getComputedStyle(body);
  let computedHeight = parseInt(cs.marginTop) + body.offsetHeight + parseInt(cs.marginBottom);
  let height = Math.max(computedHeight, PANEL_MIN_HEIGHT);
  let computedWidth = parseInt(cs.marginLeft) + body.offsetWidth + parseInt(cs.marginRight);
  let width = Math.max(computedWidth, PANEL_MIN_WIDTH);

  // The panel can only resize vertically; otherwise, we would have to
  // compensate for leftward or rightward shifts here
  iframe.style.height = height + "px";
  iframe.style.width = width + "px";
}

function ResizeWatcher(iframe) {
  this._mutationObserver = null;
  this._iframe = iframe;

  this.start();
}

ResizeWatcher.prototype = {
  start: function ResizeWatcher_start() {
    this.stop(); // just in case...
    let doc = this._iframe.contentDocument;

    this._mutationObserver = new this._iframe.contentWindow.MutationObserver(
      function(mutations) {
        sizePanelToContent(this._iframe);
      }.bind(this));

    // Observe anything that causes the size to change.
    let config = {
      attributes: true,
      characterData: true,
      childList: true,
      subtree: true
    };

    this._mutationObserver.observe(doc, config);

    // and since this may be setup after the load event has fired we do an
    // initial resize now.
    sizePanelToContent(this._iframe);
  },

  stop: function ResizeWatcher_stop() {
    if (this._mutationObserver) {
      try {
        this._mutationObserver.disconnect();
      } catch (ex) {
        // may get "TypeError: can't access dead object" which seems strange,
        // but doesn't seem to indicate a real problem, so ignore it...
      }
      this._mutationObserver = null;
    }
  }
};

/**
 * Return the chrome window and <browser> for the given outer window ID.
 */
function getUIForWindowID(aWindowId) {
  let someWindow = Services.wm.getMostRecentWindow('navigator:browser');
  if (!someWindow) {
    logger.error('SignInToWebsiteUX', 'no window');
    return {};
  }

  let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils);
  let content = windowUtils.getOuterWindowWithId(aWindowId);

  if (content) {
    let browser = content.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIWebNavigation)
                         .QueryInterface(Ci.nsIDocShell).chromeEventHandler;
    //let browser = someWindow.gBrowser;
    let chromeWin = browser.ownerDocument.defaultView;

    return {
      content: content,
      browser: browser,
      chromeWin: chromeWin
    };
  }
  logger.error('SignInToWebsiteUX', 'no content');

  return {};
}

function requestUI(aContext) {
  logger.log('requestUI for windowId', aContext.id);
  let UI = getUIForWindowID(aContext.id);

  // message is not shown in the UI but is required
  let mainAction = {
    label: UI.chromeWin.gNavigatorBundle.getString('identity.next.label'),
    accessKey: UI.chromeWin.gNavigatorBundle.getString('identity.next.accessKey'),
    callback: function() {} // required
  };
  let secondaryActions = [];
  let options = {
    context: aContext
  };

  UI.chromeWin.PopupNotifications.show(UI.browser,
                                       'identity-request', aContext.id,
                                       'identity-notification-icon', mainAction,
                                       [], options);
}

function HostFrame() {
  this._iframe = null;
  this._resizeWatcher = null;
}

HostFrame.prototype = {
  /*
   * getIframe - adds iframe to aOptions
   */
  getIframe: function HostFrame_getIframe(aOptions, aCallback) {
    if (this._gotIframe) {
      logger.error("Can only get iframe once with HostFrame helper");
      return;
    }

    this._createIframe(aOptions);
    aCallback();
  },

  cleanUp: function HostFrame_cleanUp() {
    if (this._resizeWatcher) {
      this._resizeWatcher.stop();
    }
  },

  /*
   * create an iframe and insert it into aOptions.  If showUI is
   * true, attach the iframe to a xul panel in the popup notification.
   * Otherwise attach to a hidden document.
   */
  _createIframe: function HostFrame_createIframe(aOptions) {
    let srcURI = aOptions.showUI ? kIdentityScreen : kIdentityFrame;
    logger.log('showUI is', aOptions.showUI, 'so iframe src =', srcURI);

    let hiddenDoc = Services.appShell.hiddenDOMWindow.document;
    this._iframe = hiddenDoc.createElementNS('http://www.w3.org/1999/xhtml', 'iframe');

    this._iframe.setAttribute('mozbrowser', true);
    this._iframe.setAttribute('mozframetype', 'content');
    this._iframe.setAttribute('type', 'content');
    this._iframe.setAttribute('remote', true);
    this._iframe.setAttribute('id', 'persona-host-frame');
    this._iframe.setAttribute('src', srcURI);

    // implement a dynamic resize watcher a la Social API
    this._iframe.style.height = "440px";
    this._iframe.style.width = "300px";

    aOptions.iframe = this._iframe;

    if (aOptions.showUI) {
      // synchronous, so we can call _injectShim below with no race condition
      requestUI(aOptions);
      this._resizeWatcher = new ResizeWatcher(this._iframe);
    } else {
      hiddenDoc.documentElement.appendChild(this._iframe);
    }
    this._injectShim(this._iframe);
  },

  _injectShim: function HostFrame_injectShim(aIframe) {
    let mm = aIframe.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
    logger.log('loadFrameScript:', kIdentityShim);
    mm.loadFrameScript(kIdentityShim, true);
  }
};

function Pipe(aOptions, aController) {
  this.options = aOptions;
  this.controller = aController;
  this.mm = null;
  this._closed = false;
  return this;
}

Pipe.prototype = {
  observe: function pipe_observe(aSubject, aTopic, aData) {
    logger.log('pipe observed', aTopic);
    switch (aTopic) {
      case 'identity-delegate-canceled':
        this._close();
        this.controller.serviceDoMethod({method: 'cancel'}, this.options.id);
        break;

      default:
        logger.error('pipe observed unexpected topic: ' + aTopic);
        break;
    }
  },

  _close: function pipe__delegateClose() {
    this._closed = true;
    Services.obs.removeObserver(this, 'identity-delegate-canceled');
    if (this.mm) {
      this.mm.removeMessageListener('identity-service-doMethod', this._serviceDoMethod);
      this.mm.removeMessageListener('identity-delegate-complete', this._delegateComplete);
      this.mm.removeMessageListener('identity-delegate-loaded', this._delegateLoaded);
    }
    let subject = Cc['@mozilla.org/supports-string;1'].createInstance(Ci.nsISupportsString);
    subject.data = this.options.id;
    Services.obs.notifyObservers(subject, 'identity-delegate-ui-close', null);

    if (typeof this.options.onComplete === 'function') {
      this.options.onComplete();
    }
  },

  _delegateLoaded: function pipe__delegateLoaded() {
    this.mm.sendAsyncMessage(this.options.message, this.options.rpOptions);
    //this.resizer = new DynamicResizeWatcher();
    //this.resizer.start(
  },

  _delegateComplete: function pipe__delegateComplete() {
    this._close();
  },

  _serviceDoMethod: function pipe__doMethod(aMethodOptions) {
    let message = aMethodOptions.json;
    if (typeof message === 'string') {
      try {
        message = JSON.parse(message);
      } catch (err) {
        logger.error('Bad json message: ' + message);
        return;
      }
    }
    this.controller.serviceDoMethod(message, this.options.id);
  },

  communicate: function pipe_communicate() {
    if (this._closed) {
      logger.error('Cannot communicate with persona frame; pipe already closed');
      return;
    }
    Services.obs.addObserver(this, 'identity-delegate-canceled', false);

    let frameLoader = this.options.iframe.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    if (frameLoader) {
      this.mm = frameLoader.messageManager;
      this.mm.addMessageListener('identity-service-doMethod', this._serviceDoMethod.bind(this));
      this.mm.addMessageListener('identity-delegate-loaded', this._delegateLoaded.bind(this));
      this.mm.addMessageListener('identity-delegate-complete', this._delegateComplete.bind(this));
    } else {
      logger.error('FrameLoader unavailable; Frame did not get attached properly?');
    }
  }
};

this.SignInToWebsiteUX = {
  init: function SignInToWebsiteUX_init() {
    this.contexts = {};
    Services.obs.addObserver(this, 'identity-controller-watch', false);
    Services.obs.addObserver(this, 'identity-controller-request', false);
    Services.obs.addObserver(this, 'identity-controller-logout', false);
    Services.obs.addObserver(this, 'identity-controller-canceled', false);
  },

  uninit: function SignInToWebsiteUX_uninit() {
    Services.obs.removeObserver(this, 'identity-controller-watch');
    Services.obs.removeObserver(this, 'identity-controller-request');
    Services.obs.removeObserver(this, 'identity-controller-logout');
    Services.obs.removeObserver(this, 'identity-controller-canceled');
  },

  observe: function SignInToWebsiteUX_observe(aSubject, aTopic, aData) {
    logger.log('controller observed:', aTopic);
    // XXX need to detect page unload of any of our flows
    // XXX we get strings from xul, and objects from elsewhere
    let rpOptions = {};
    if (aSubject) {
      if (aSubject.wrappedJSObject) {
        rpOptions = aSubject.wrappedJSObject;
      } else {
        rpOptions = {id: aSubject.QueryInterface(Ci.nsISupportsString).data};
      }
    }
    if (!rpOptions.id) {
      logger.error('Got a message with no RP id');
      return;
    }

    let rpId = rpOptions.id;
    let UI = getUIForWindowID(rpId);

    let options = {
      id: rpOptions.id,
      rpOptions: rpOptions
    };

    switch (aTopic) {
      case 'identity-controller-watch':
        this.doWatch(options);
        break;

      case 'identity-controller-request':
        this.doRequest(options);
        break;

      case 'identity-controller-logout':
        this.doLogout(options);
        break;

      default:
        logger.error('SignInToWebsiteUX', 'Unknown observer notification:', aTopic);
        break;
    }
  },

  serviceDoMethod: function SignInToWebsiteUX_doMethod(aMessage, aId) {
    logger.log('serviceDoMethod received:', aMessage);
    switch (aMessage.method) {
      case 'ready':
        IdentityService.doReady(aId);
        break;

      case 'login':
        if (aMessage._internalParams) {
          IdentityService.doLogin(aId, aMessage.assertion, aMessage._internalParams);
        } else {
          IdentityService.doLogin(aId, aMessage.assertion);
        }
        break;

      case 'logout':
        IdentityService.doLogout(aId);
        break;

      case 'cancel':
        IdentityService.doCancel(aId);
        break;

      default:
        logger.error('Unknown identity method: ' + aMessage.method);
        break;
    }
  },

  cleanUp: function SignInToWebsiteUX_cleanUp(aId) {
    let context = this.contexts[aId];
    if (context) {
      if (context.hostFrame) {
        context.hostFrame.cleanUp();
      }
      if (context.iframe && context.iframe.parentNode) {
        logger.log("removing iframe from parent node and deleting it");
        context.iframe.parentNode.removeChild(context.iframe);
        delete context.iframe;
      }
      this.contexts[aId] = {};
      delete this.contexts[aId];
    }
  },

  delegate: function SignInToWebsiteUX_delegate(aOptions) {
    let hostFrame = new HostFrame();
    hostFrame.getIframe(aOptions, function() {
      // iframe has been added to aOptions

      // callback for the pipe when flow is complete
      aOptions.onComplete = function pipe_onComplete() {
        this.cleanUp(aOptions.id);
      }.bind(this);

      // store context and communicate with pipe
      this.contexts[aOptions.id] = aOptions;
      this.contexts[aOptions.id].hostFrame = hostFrame;

      let pipe = new Pipe(aOptions, this);
      pipe.communicate();
    }.bind(this));
  },

  doWatch: function SignInToWebsiteUX_doWatch(aOptions) {
    aOptions.message = 'identity-delegate-watch';
    aOptions.showUI = false;
    this.delegate(aOptions);
  },

  doRequest: function SignInToWebsiteUX_doRequest(aOptions) {
    aOptions.message = 'identity-delegate-request';
    aOptions.showUI = true;
    this.delegate(aOptions);
  },

  doLogout: function SignInToWebsiteUX_doLogout(aOptions) {
    aOptions.message = 'identity-delegate-logout';
    aOptions.showUI = false;
    this.delegate(aOptions);
  }
};
