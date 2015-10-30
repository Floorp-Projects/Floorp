/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "experimental"
};

const { Ci } = require("chrome");
const XUL = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';

function eventTarget(frame) {
  return getDocShell(frame).chromeEventHandler;
}
exports.eventTarget = eventTarget;

function getDocShell(frame) {
  let { frameLoader } = frame.QueryInterface(Ci.nsIFrameLoaderOwner);
  return frameLoader && frameLoader.docShell;
}
exports.getDocShell = getDocShell;

/**
 * Creates a XUL `browser` element in a privileged document.
 * @params {nsIDOMDocument} document
 * @params {String} options.type
 *    By default is 'content' for possible values see:
 *    https://developer.mozilla.org/en/XUL/iframe#a-browser.type
 * @params {String} options.uri
 *    URI of the document to be loaded into created frame.
 * @params {Boolean} options.remote
 *    If `true` separate process will be used for this frame, also in such
 *    case all the following options are ignored.
 * @params {Boolean} options.allowAuth
 *    Whether to allow auth dialogs. Defaults to `false`.
 * @params {Boolean} options.allowJavascript
 *    Whether to allow Javascript execution. Defaults to `false`.
 * @params {Boolean} options.allowPlugins
 *    Whether to allow plugin execution. Defaults to `false`.
 */
function create(target, options) {
  target = target instanceof Ci.nsIDOMDocument ? target.documentElement :
           target instanceof Ci.nsIDOMWindow ? target.document.documentElement :
           target;
  options = options || {};
  let remote = options.remote || false;
  let namespaceURI = options.namespaceURI || XUL;
  let isXUL = namespaceURI === XUL;
  let nodeName = isXUL && options.browser ? 'browser' : 'iframe';
  let document = target.ownerDocument;

  let frame = document.createElementNS(namespaceURI, nodeName);
  // Type="content" is mandatory to enable stuff here:
  // http://mxr.mozilla.org/mozilla-central/source/content/base/src/nsFrameLoader.cpp#1776
  frame.setAttribute('type', options.type || 'content');
  frame.setAttribute('src', options.uri || 'about:blank');

  // Must set the remote attribute before attaching the frame to the document
  if (remote && isXUL) {
    // We remove XBL binding to avoid execution of code that is not going to
    // work because browser has no docShell attribute in remote mode
    // (for example)
    frame.setAttribute('style', '-moz-binding: none;');
    frame.setAttribute('remote', 'true');
  }

  target.appendChild(frame);

  // Load in separate process if `options.remote` is `true`.
  // http://mxr.mozilla.org/mozilla-central/source/content/base/src/nsFrameLoader.cpp#1347
  if (remote && !isXUL) {
    frame.QueryInterface(Ci.nsIMozBrowserFrame);
    frame.createRemoteFrameLoader(null);
  }

  // If browser is remote it won't have a `docShell`.
  if (!remote) {
    let docShell = getDocShell(frame);
    docShell.allowAuth = options.allowAuth || false;
    docShell.allowJavascript = options.allowJavascript || false;
    docShell.allowPlugins = options.allowPlugins || false;
    docShell.allowWindowControl = options.allowWindowControl || false;
  }

  return frame;
}
exports.create = create;

function swapFrameLoaders(from, to) {
  return from.QueryInterface(Ci.nsIFrameLoaderOwner).swapFrameLoaders(to);
}
exports.swapFrameLoaders = swapFrameLoaders;
