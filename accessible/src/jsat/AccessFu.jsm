/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

this.EXPORTED_SYMBOLS = ['AccessFu'];

Cu.import('resource://gre/modules/Services.jsm');

Cu.import('resource://gre/modules/accessibility/Utils.jsm');

const ACCESSFU_DISABLE = 0;
const ACCESSFU_ENABLE = 1;
const ACCESSFU_AUTO = 2;

this.AccessFu = {
  /**
   * Initialize chrome-layer accessibility functionality.
   * If accessibility is enabled on the platform, then a special accessibility
   * mode is started.
   */
  attach: function attach(aWindow) {
    if (this.chromeWin)
      // XXX: only supports attaching to one window now.
      throw new Error('Only one window could be attached to AccessFu');

    this.chromeWin = aWindow;

    this.prefsBranch = Cc['@mozilla.org/preferences-service;1']
      .getService(Ci.nsIPrefService).getBranch('accessibility.accessfu.');
    this.prefsBranch.addObserver('activate', this, false);

    try {
      Cc['@mozilla.org/android/bridge;1'].
        getService(Ci.nsIAndroidBridge).handleGeckoMessage(
          JSON.stringify({ gecko: { type: 'Accessibility:Ready' } }));
      Services.obs.addObserver(this, 'Accessibility:Settings', false);
    } catch (x) {
      // Not on Android
      aWindow.addEventListener(
        'ContentStart',
        (function(event) {
           let content = aWindow.shell.contentBrowser.contentWindow;
           content.addEventListener('mozContentEvent', this, false, true);
         }).bind(this), false);
    }

    try {
      this._activatePref = this.prefsBranch.getIntPref('activate');
    } catch (x) {
      this._activatePref = ACCESSFU_DISABLE;
    }

    Input.quickNavMode.updateModes(this.prefsBranch);

    this._enableOrDisable();
  },

  /**
   * Start AccessFu mode, this primarily means controlling the virtual cursor
   * with arrow keys.
   */
  _enable: function _enable() {
    if (this._enabled)
      return;
    this._enabled = true;

    Cu.import('resource://gre/modules/accessibility/Utils.jsm');
    Cu.import('resource://gre/modules/accessibility/TouchAdapter.jsm');
    Cu.import('resource://gre/modules/accessibility/Presentation.jsm');

    Logger.info('enable');

    for each (let mm in Utils.getAllMessageManagers(this.chromeWin))
      this._loadFrameScript(mm);

    // Add stylesheet
    let stylesheetURL = 'chrome://global/content/accessibility/AccessFu.css';
    this.stylesheet = this.chromeWin.document.createProcessingInstruction(
      'xml-stylesheet', 'href="' + stylesheetURL + '" type="text/css"');
    this.chromeWin.document.insertBefore(this.stylesheet,
                                         this.chromeWin.document.firstChild);

    Input.attach(this.chromeWin);
    Output.attach(this.chromeWin);
    TouchAdapter.attach(this.chromeWin);

    Services.obs.addObserver(this, 'remote-browser-frame-shown', false);
    Services.obs.addObserver(this, 'Accessibility:NextObject', false);
    Services.obs.addObserver(this, 'Accessibility:PreviousObject', false);
    Services.obs.addObserver(this, 'Accessibility:Focus', false);
    this.chromeWin.addEventListener('TabOpen', this);
    this.chromeWin.addEventListener('TabSelect', this);
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  _disable: function _disable() {
    if (!this._enabled)
      return;

    this._enabled = false;

    Logger.info('disable');

    this.chromeWin.document.removeChild(this.stylesheet);
    for each (let mm in Utils.getAllMessageManagers(this.chromeWin))
      mm.sendAsyncMessage('AccessFu:Stop');

    Input.detach();
    TouchAdapter.detach(this.chromeWin);

    this.chromeWin.removeEventListener('TabOpen', this);
    this.chromeWin.removeEventListener('TabSelect', this);

    Services.obs.removeObserver(this, 'remote-browser-frame-shown');
    Services.obs.removeObserver(this, 'Accessibility:NextObject');
    Services.obs.removeObserver(this, 'Accessibility:PreviousObject');
    Services.obs.removeObserver(this, 'Accessibility:Focus');
  },

  _enableOrDisable: function _enableOrDisable() {
    try {
      if (this._activatePref == ACCESSFU_ENABLE ||
          this._systemPref && this._activatePref == ACCESSFU_AUTO)
        this._enable();
      else
        this._disable();
    } catch (x) {
      dump('Error ' + x.message + ' ' + x.fileName + ':' + x.lineNumber);
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    if (Logger.logLevel >= Logger.DEBUG)
      Logger.debug('Recieved', aMessage.name, JSON.stringify(aMessage.json));

    switch (aMessage.name) {
      case 'AccessFu:Ready':
        let mm = Utils.getMessageManager(aMessage.target);
        mm.sendAsyncMessage('AccessFu:Start',
                            {method: 'start', buildApp: Utils.MozBuildApp});
        break;
      case 'AccessFu:Present':
        this._output(aMessage.json, aMessage.target);
        break;
      case 'AccessFu:Input':
        Input.setEditState(aMessage.json);
        break;
    }
  },

  _output: function _output(aPresentationData, aBrowser) {
      try {
        for each (let presenter in aPresentationData) {
          if (!presenter)
            continue;

          Output[presenter.type](presenter.details, aBrowser);
        }
      } catch (x) {
        Logger.logException(x);
      }
  },

  _loadFrameScript: function _loadFrameScript(aMessageManager) {
    aMessageManager.addMessageListener('AccessFu:Present', this);
    aMessageManager.addMessageListener('AccessFu:Input', this);
    aMessageManager.addMessageListener('AccessFu:Ready', this);
    aMessageManager.
      loadFrameScript(
        'chrome://global/content/accessibility/content-script.js', true);
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case 'Accessibility:Settings':
        this._systemPref = JSON.parse(aData).enabled;
        this._enableOrDisable();
        break;
      case 'Accessibility:NextObject':
        Input.moveCursor('moveNext', 'Simple', 'gesture');
        break;
      case 'Accessibility:PreviousObject':
        Input.moveCursor('movePrevious', 'Simple', 'gesture');
        break;
      case 'Accessibility:Focus':
        this._focused = JSON.parse(aData);
        if (this._focused) {
          let mm = Utils.getMessageManager(Utils.getCurrentBrowser(this.chromeWin));
          mm.sendAsyncMessage('AccessFu:VirtualCursor',
                              {action: 'whereIsIt', move: true});
        }
        break;
      case 'nsPref:changed':
        if (aData == 'activate') {
          this._activatePref = this.prefsBranch.getIntPref('activate');
          this._enableOrDisable();
        } else if (aData == 'quicknav_modes') {
          Input.quickNavMode.updateModes(this.prefsBranch);
        }
        break;
      case 'remote-browser-frame-shown':
      {
        this._loadFrameScript(
          aSubject.QueryInterface(Ci.nsIFrameLoader).messageManager);
        break;
      }
    }
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'mozContentEvent':
      {
        if (aEvent.detail.type == 'accessibility-screenreader') {
          this._systemPref = aEvent.detail.enabled;
          this._enableOrDisable();
        }
        break;
      }
      case 'TabOpen':
      {
        this._loadFrameScript(Utils.getMessageManager(aEvent.target));
        break;
      }
      case 'TabSelect':
      {
        if (this._focused) {
          let mm = Utils.getMessageManager(Utils.getCurrentBrowser(this.chromeWin));
          // We delay this for half a second so the awesomebar could close,
          // and we could use the current coordinates for the content item.
          // XXX TODO figure out how to avoid magic wait here.
          this.chromeWin.setTimeout(
            function () {
              mm.sendAsyncMessage('AccessFu:VirtualCursor', {action: 'whereIsIt'});
            }, 500);
        }
        break;
      }
    }
  },

  announce: function announce(aAnnouncement) {
    this._output(Presentation.announce(aAnnouncement),
                 Utils.getCurrentBrowser(this.chromeWin));
  },

  // So we don't enable/disable twice
  _enabled: false,

  // Layerview is focused
  _focused: false
};

var Output = {
  attach: function attach(aWindow) {
    this.chromeWin = aWindow;
    Cu.import('resource://gre/modules/Geometry.jsm');
  },

  Speech: function Speech(aDetails, aBrowser) {
    for each (let action in aDetails.actions)
      Logger.info('tts.' + action.method, '"' + action.data + '"', JSON.stringify(action.options));
  },

  Visual: function Visual(aDetails, aBrowser) {
    switch (aDetails.method) {
      case 'showBounds':
      {
        if (!this.highlightBox) {
          // Add highlight box
          this.highlightBox = this.chromeWin.document.
            createElementNS('http://www.w3.org/1999/xhtml', 'div');
          this.chromeWin.document.documentElement.appendChild(this.highlightBox);
          this.highlightBox.id = 'virtual-cursor-box';

          // Add highlight inset for inner shadow
          let inset = this.chromeWin.document.
            createElementNS('http://www.w3.org/1999/xhtml', 'div');
          inset.id = 'virtual-cursor-inset';

          this.highlightBox.appendChild(inset);
        }

        let padding = aDetails.padding;
        let r = this._adjustBounds(aDetails.bounds, aBrowser);

        // First hide it to avoid flickering when changing the style.
        this.highlightBox.style.display = 'none';
        this.highlightBox.style.top = (r.top - padding) + 'px';
        this.highlightBox.style.left = (r.left - padding) + 'px';
        this.highlightBox.style.width = (r.width + padding*2) + 'px';
        this.highlightBox.style.height = (r.height + padding*2) + 'px';
        this.highlightBox.style.display = 'block';

        break;
      }
      case 'hideBounds':
      {
        if (this.highlightBox)
          this.highlightBox.style.display = 'none';
        break;
      }
      case 'showAnnouncement':
      {
        if (!this.announceBox) {
          this.announceBox = this.chromeWin.document.
            createElementNS('http://www.w3.org/1999/xhtml', 'div');
          this.announceBox.id = 'announce-box';
          this.chromeWin.document.documentElement.appendChild(this.announceBox);
        }

        this.announceBox.innerHTML = '<div>' + aDetails.text + '</div>';
        this.announceBox.classList.add('showing');

        if (this._announceHideTimeout)
          this.chromeWin.clearTimeout(this._announceHideTimeout);

        if (aDetails.duration > 0)
          this._announceHideTimeout = this.chromeWin.setTimeout(
            function () {
              this.announceBox.classList.remove('showing');
              this._announceHideTimeout = 0;
            }.bind(this), aDetails.duration);
        break;
      }
      case 'hideAnnouncement':
      {
        this.announceBox.classList.remove('showing');
        break;
      }
    }
  },

  Android: function Android(aDetails, aBrowser) {
    if (!this._bridge)
      this._bridge = Cc['@mozilla.org/android/bridge;1'].getService(Ci.nsIAndroidBridge);

    for each (let androidEvent in aDetails) {
      androidEvent.type = 'Accessibility:Event';
      if (androidEvent.bounds)
        androidEvent.bounds = this._adjustBounds(androidEvent.bounds, aBrowser);
      this._bridge.handleGeckoMessage(JSON.stringify({gecko: androidEvent}));
    }
  },

  Haptic: function Haptic(aDetails, aBrowser) {
    this.chromeWin.navigator.vibrate(aDetails.pattern);
  },

  _adjustBounds: function(aJsonBounds, aBrowser) {
    let bounds = new Rect(aJsonBounds.left, aJsonBounds.top,
                          aJsonBounds.right - aJsonBounds.left,
                          aJsonBounds.bottom - aJsonBounds.top);
    let vp = Utils.getViewport(this.chromeWin) || { zoom: 1.0, offsetY: 0 };
    let browserOffset = aBrowser.getBoundingClientRect();

    return bounds.translate(browserOffset.left, browserOffset.top).
      scale(vp.zoom, vp.zoom).expandToIntegers();
  }
};

var Input = {
  editState: {},

  attach: function attach(aWindow) {
    this.chromeWin = aWindow;
    this.chromeWin.document.addEventListener('keypress', this, true);
    this.chromeWin.addEventListener('mozAccessFuGesture', this, true);
  },

  detach: function detach() {
    this.chromeWin.document.removeEventListener('keypress', this, true);
    this.chromeWin.removeEventListener('mozAccessFuGesture', this, true);
  },

  handleEvent: function Input_handleEvent(aEvent) {
    try {
      switch (aEvent.type) {
      case 'keypress':
        this._handleKeypress(aEvent);
        break;
      case 'mozAccessFuGesture':
        this._handleGesture(aEvent.detail);
        break;
      }
    } catch (x) {
      Logger.logException(x);
    }
  },

  _handleGesture: function _handleGesture(aGesture) {
    let gestureName = aGesture.type + aGesture.touches.length;
    Logger.info('Gesture', aGesture.type,
                '(fingers: ' + aGesture.touches.length + ')');

    switch (gestureName) {
      case 'dwell1':
      case 'explore1':
        this.moveCursor('moveToPoint', 'Simple', 'gesture',
                        aGesture.x, aGesture.y);
        break;
      case 'doubletap1':
        this.activateCurrent();
        break;
      case 'swiperight1':
        this.moveCursor('moveNext', 'Simple', 'gestures');
        break;
      case 'swipeleft1':
        this.moveCursor('movePrevious', 'Simple', 'gesture');
        break;
      case 'swiperight2':
        this.scroll(-1, true);
        break;
      case 'swipedown2':
        this.scroll(-1);
        break;
      case 'swipeleft2':
        this.scroll(1, true);
        break;
      case 'swipeup2':
        this.scroll(1);
        break;
      case 'explore2':
        Utils.getCurrentBrowser(this.chromeWin).contentWindow.scrollBy(
          -aGesture.deltaX, -aGesture.deltaY);
        break;
      case 'swiperight3':
        this.moveCursor('moveNext', this.quickNavMode.current, 'gesture');
        break;
      case 'swipeleft3':
        this.moveCursor('movePrevious', this.quickNavMode.current, 'gesture');
        break;
      case 'swipedown3':
        this.quickNavMode.next();
        AccessFu.announce('quicknav_' + this.quickNavMode.current);
        break;
      case 'swipeup3':
        this.quickNavMode.previous();
        AccessFu.announce('quicknav_' + this.quickNavMode.current);
        break;
    }
  },

  _handleKeypress: function _handleKeypress(aEvent) {
    let target = aEvent.target;

    // Ignore keys with modifiers so the content could take advantage of them.
    if (aEvent.ctrlKey || aEvent.altKey || aEvent.metaKey)
      return;

    switch (aEvent.keyCode) {
      case 0:
        // an alphanumeric key was pressed, handle it separately.
        // If it was pressed with either alt or ctrl, just pass through.
        // If it was pressed with meta, pass the key on without the meta.
        if (this.editState.editing)
          return;

        let key = String.fromCharCode(aEvent.charCode);
        try {
          let [methodName, rule] = this.keyMap[key];
          this.moveCursor(methodName, rule, 'keyboard');
        } catch (x) {
          return;
        }
        break;
      case aEvent.DOM_VK_RIGHT:
        if (this.editState.editing) {
          if (!this.editState.atEnd)
            // Don't move forward if caret is not at end of entry.
            // XXX: Fix for rtl
            return;
          else
            target.blur();
        }
        this.moveCursor(aEvent.shiftKey ? 'moveLast' : 'moveNext', 'Simple', 'keyboard');
        break;
      case aEvent.DOM_VK_LEFT:
        if (this.editState.editing) {
          if (!this.editState.atStart)
            // Don't move backward if caret is not at start of entry.
            // XXX: Fix for rtl
            return;
          else
            target.blur();
        }
        this.moveCursor(aEvent.shiftKey ? 'moveFirst' : 'movePrevious', 'Simple', 'keyboard');
        break;
      case aEvent.DOM_VK_UP:
        if (this.editState.multiline) {
          if (!this.editState.atStart)
            // Don't blur content if caret is not at start of text area.
            return;
          else
            target.blur();
        }

        if (Utils.MozBuildApp == 'mobile/android')
          // Return focus to native Android browser chrome.
          Cc['@mozilla.org/android/bridge;1'].
            getService(Ci.nsIAndroidBridge).handleGeckoMessage(
              JSON.stringify({ gecko: { type: 'ToggleChrome:Focus' } }));
        break;
      case aEvent.DOM_VK_RETURN:
      case aEvent.DOM_VK_ENTER:
        if (this.editState.editing)
          return;
        this.activateCurrent();
        break;
    default:
      return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  moveCursor: function moveCursor(aAction, aRule, aInputType, aX, aY) {
    let mm = Utils.getMessageManager(Utils.getCurrentBrowser(this.chromeWin));
    mm.sendAsyncMessage('AccessFu:VirtualCursor',
                        {action: aAction, rule: aRule,
                         x: aX, y: aY, origin: 'top',
                         inputType: aInputType});
  },

  activateCurrent: function activateCurrent() {
    let mm = Utils.getMessageManager(Utils.getCurrentBrowser(this.chromeWin));
    mm.sendAsyncMessage('AccessFu:Activate', {});
  },

  setEditState: function setEditState(aEditState) {
    this.editState = aEditState;
  },

  scroll: function scroll(aPage, aHorizontal) {
    let mm = Utils.getMessageManager(Utils.getCurrentBrowser(this.chromeWin));
    mm.sendAsyncMessage('AccessFu:Scroll', {page: aPage, horizontal: aHorizontal, origin: 'top'});
  },

  get keyMap() {
    delete this.keyMap;
    this.keyMap = {
      a: ['moveNext', 'Anchor'],
      A: ['movePrevious', 'Anchor'],
      b: ['moveNext', 'Button'],
      B: ['movePrevious', 'Button'],
      c: ['moveNext', 'Combobox'],
      C: ['movePrevious', 'Combobox'],
      e: ['moveNext', 'Entry'],
      E: ['movePrevious', 'Entry'],
      f: ['moveNext', 'FormElement'],
      F: ['movePrevious', 'FormElement'],
      g: ['moveNext', 'Graphic'],
      G: ['movePrevious', 'Graphic'],
      h: ['moveNext', 'Heading'],
      H: ['movePrevious', 'Heading'],
      i: ['moveNext', 'ListItem'],
      I: ['movePrevious', 'ListItem'],
      k: ['moveNext', 'Link'],
      K: ['movePrevious', 'Link'],
      l: ['moveNext', 'List'],
      L: ['movePrevious', 'List'],
      p: ['moveNext', 'PageTab'],
      P: ['movePrevious', 'PageTab'],
      r: ['moveNext', 'RadioButton'],
      R: ['movePrevious', 'RadioButton'],
      s: ['moveNext', 'Separator'],
      S: ['movePrevious', 'Separator'],
      t: ['moveNext', 'Table'],
      T: ['movePrevious', 'Table'],
      x: ['moveNext', 'Checkbox'],
      X: ['movePrevious', 'Checkbox']
    };

    return this.keyMap;
  },

  quickNavMode: {
    get current() {
      return this.modes[this._currentIndex];
    },

    previous: function quickNavMode_previous() {
      if (--this._currentIndex < 0)
        this._currentIndex = this.modes.length - 1;
    },

    next: function quickNavMode_next() {
      if (++this._currentIndex >= this.modes.length)
        this._currentIndex = 0;
    },

    updateModes: function updateModes(aPrefsBranch) {
      try {
        this.modes = aPrefsBranch.getCharPref('quicknav_modes').split(',');
      } catch (x) {
        // Fallback
        this.modes = [];
      }
    },

    _currentIndex: -1
  }
};
