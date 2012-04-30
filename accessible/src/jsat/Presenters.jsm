/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/accessibility/UtteranceGenerator.jsm');
Cu.import('resource://gre/modules/Services.jsm');

var EXPORTED_SYMBOLS = ['VisualPresenter',
                        'AndroidPresenter',
                        'DummyAndroidPresenter'];

/**
 * The interface for all presenter classes. A presenter could be, for example,
 * a speech output module, or a visual cursor indicator.
 */
function Presenter() {}

Presenter.prototype = {
  /**
   * Attach function for presenter.
   * @param {ChromeWindow} aWindow Chrome window the presenter could use.
   */
  attach: function attach(aWindow) {},

  /**
   * Detach function.
   */
  detach: function detach() {},

  /**
   * The virtual cursor's position changed.
   * @param {nsIAccessible} aObject the new position.
   * @param {nsIAccessible[]} aNewContext the ancestry of the new position that
   *    is different from the old virtual cursor position.
   */
  pivotChanged: function pivotChanged(aObject, aNewContext) {},

  /**
   * An object's action has been invoked.
   * @param {nsIAccessible} aObject the object that has been invoked.
   * @param {string} aActionName the name of the action.
   */
  actionInvoked: function actionInvoked(aObject, aActionName) {},

  /**
   * Text has changed, either by the user or by the system. TODO.
   */
  textChanged: function textChanged() {},

  /**
   * Text selection has changed. TODO.
   */
  textSelectionChanged: function textSelectionChanged() {},

  /**
   * Selection has changed. TODO.
   * @param {nsIAccessible} aObject the object that has been selected.
   */
  selectionChanged: function selectionChanged(aObject) {},

  /**
   * The page state has changed, loading, stopped loading, etc. TODO.
   */
  pageStateChanged: function pageStateChanged() {},

  /**
   * The tab has changed.
   * @param {nsIAccessible} aObject the document contained in the tab.
   */
  tabSelected: function tabSelected(aObject) {},

  /**
   * The viewport has changed, either a scroll, pan, zoom, or
   *    landscape/portrait toggle.
   */
  viewportChanged: function viewportChanged() {}
};

/**
 * Visual presenter. Draws a box around the virtual cursor's position.
 */

function VisualPresenter() {}

VisualPresenter.prototype = new Presenter();

/**
 * The padding in pixels between the object and the highlight border.
 */
VisualPresenter.prototype.BORDER_PADDING = 2;

VisualPresenter.prototype.attach = function(aWindow) {
  this.chromeWin = aWindow;

  // Add stylesheet
  let stylesheetURL = 'chrome://global/content/accessibility/AccessFu.css';
  this.stylesheet = aWindow.document.createProcessingInstruction(
    'xml-stylesheet', 'href="' + stylesheetURL + '" type="text/css"');
  aWindow.document.insertBefore(this.stylesheet, aWindow.document.firstChild);

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
};

VisualPresenter.prototype.detach = function() {
  this.chromeWin.document.removeChild(this.stylesheet);
  this.highlightBox.parentNode.removeChild(this.highlightBox);
  this.highlightBox = this.stylesheet = null;
};

VisualPresenter.prototype.viewportChanged = function() {
  if (this._currentObject)
    this.highlight(this._currentObject);
};

VisualPresenter.prototype.pivotChanged = function(aObject, aNewContext) {
  this._currentObject = aObject;

  if (!aObject) {
    this.hide();
    return;
  }

  try {
    aObject.scrollTo(Ci.nsIAccessibleScrollType.SCROLL_TYPE_ANYWHERE);
    this.highlight(aObject);
  } catch (e) {
    dump('Error getting bounds: ' + e);
    return;
  }
};

VisualPresenter.prototype.tabSelected = function(aObject) {
  let vcDoc = aObject.QueryInterface(Ci.nsIAccessibleCursorable);
  this.pivotChanged(vcDoc.virtualCursor.position);
};

// Internals

VisualPresenter.prototype.hide = function hide() {
  this.highlightBox.style.display = 'none';
};

VisualPresenter.prototype.highlight = function(aObject) {
  let vp = (Services.appinfo.OS == 'Android') ?
    this.chromeWin.BrowserApp.selectedTab.getViewport() :
    { zoom: 1.0, offsetY: 0 };

  let bounds = this.getBounds(aObject, vp.zoom);

  // First hide it to avoid flickering when changing the style.
  this.highlightBox.style.display = 'none';
  this.highlightBox.style.top = bounds.top + 'px';
  this.highlightBox.style.left = bounds.left + 'px';
  this.highlightBox.style.width = bounds.width + 'px';
  this.highlightBox.style.height = bounds.height + 'px';
  this.highlightBox.style.display = 'block';
};

VisualPresenter.prototype.getBounds = function(aObject, aZoom, aStart, aEnd) {
  let objX = {}, objY = {}, objW = {}, objH = {};

  if (aEnd >= 0 && aStart >= 0 && aEnd != aStart) {
    // TODO: Get bounds for text ranges. Leaving this blank until we have
    // proper text navigation in the virtual cursor.
  }

  aObject.getBounds(objX, objY, objW, objH);

  // Can't specify relative coords in nsIAccessible.getBounds, so we do it.
  let docX = {}, docY = {};
  let docRoot = aObject.rootDocument.QueryInterface(Ci.nsIAccessible);
  docRoot.getBounds(docX, docY, {}, {});

  let rv = {
    left: Math.round((objX.value - docX.value - this.BORDER_PADDING) * aZoom),
    top: Math.round((objY.value - docY.value - this.BORDER_PADDING) * aZoom),
    width: Math.round((objW.value + (this.BORDER_PADDING * 2)) * aZoom),
    height: Math.round((objH.value + (this.BORDER_PADDING * 2)) * aZoom)
  };

  return rv;
};

/**
 * Android presenter. Fires Android a11y events.
 */

const ANDROID_TYPE_VIEW_CLICKED = 0x01;
const ANDROID_TYPE_VIEW_LONG_CLICKED = 0x02;
const ANDROID_TYPE_VIEW_SELECTED = 0x04;
const ANDROID_TYPE_VIEW_FOCUSED = 0x08;
const ANDROID_TYPE_VIEW_TEXT_CHANGED = 0x10;
const ANDROID_TYPE_WINDOW_STATE_CHANGED = 0x20;

function AndroidPresenter() {}

AndroidPresenter.prototype = new Presenter();

AndroidPresenter.prototype.pivotChanged = function(aObject, aNewContext) {
  let output = [];
  for (let i in aNewContext)
    output.push.apply(output,
                      UtteranceGenerator.genForObject(aNewContext[i]));

  output.push.apply(output,
                    UtteranceGenerator.genForObject(aObject, true));

  this.sendMessageToJava({
    gecko: {
      type: 'Accessibility:Event',
      eventType: ANDROID_TYPE_VIEW_FOCUSED,
      text: output
    }
  });
};

AndroidPresenter.prototype.actionInvoked = function(aObject, aActionName) {
  this.sendMessageToJava({
    gecko: {
      type: 'Accessibility:Event',
      eventType: ANDROID_TYPE_VIEW_CLICKED,
      text: UtteranceGenerator.genForAction(aObject, aActionName)
    }
  });
};

AndroidPresenter.prototype.tabSelected = function(aObject) {
  let vcDoc = aObject.QueryInterface(Ci.nsIAccessibleCursorable);
  let context = [];

  let parent = vcDoc.virtualCursor.position || aObject;
  while ((parent = parent.parent))
    context.push(parent);
  context.reverse();

  this.pivotChanged(vcDoc.virtualCursor.position || aObject, context);
};

AndroidPresenter.prototype.sendMessageToJava = function(aMessage) {
  return Cc['@mozilla.org/android/bridge;1'].
    getService(Ci.nsIAndroidBridge).
    handleGeckoMessage(JSON.stringify(aMessage));
};

/**
 * A dummy Android presenter for desktop testing
 */

function DummyAndroidPresenter() {}

DummyAndroidPresenter.prototype = new AndroidPresenter();

DummyAndroidPresenter.prototype.sendMessageToJava = function(aMessage) {
  dump(JSON.stringify(aMessage, null, 2) + '\n');
};
