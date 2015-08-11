/*
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var EXPORTED_SYMBOLS = ['getStartupInfo', 'parseQueryString', 'isContentWindowPrivate'];

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/Services.jsm');

Components.utils.import('chrome://shumway/content/ShumwayCom.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'PrivateBrowsingUtils',
  'resource://gre/modules/PrivateBrowsingUtils.jsm');

function flashUnescape(s) {
  return decodeURIComponent(s.split('+').join(' '));
}

function parseQueryString(qs) {
  if (!qs)
    return {};

  if (qs.charAt(0) == '?')
    qs = qs.slice(1);

  var values = qs.split('&');
  var obj = {};
  for (var i = 0; i < values.length; i++) {
    var pair = values[i], j = pair.indexOf('=');
    if (j < 0) {
      continue; // skipping invalid values
    }
    var key = pair.substring(0, j), value = pair.substring(j + 1);
    obj[flashUnescape(key)] = flashUnescape(value);
  }

  return obj;
}

function isContentWindowPrivate(win) {
  if (!('isContentWindowPrivate' in PrivateBrowsingUtils)) {
    return PrivateBrowsingUtils.isWindowPrivate(win);
  }
  return PrivateBrowsingUtils.isContentWindowPrivate(win);
}

function isStandardEmbedWrapper(embedElement) {
  try {
    if (embedElement.tagName !== 'EMBED') {
      return false;
    }
    var swfUrl = embedElement.src;
    var document = embedElement.ownerDocument;
    var docUrl = document.location.href;
    if (swfUrl !== docUrl) {
      return false; // document URL shall match embed src
    }
    if (document.body.children.length !== 1 ||
      document.body.firstChild !== embedElement) {
      return false; // not the only child
    }
    if (document.defaultView.top !== document.defaultView) {
      return false; // not a top window
    }
    // Looks like a standard wrapper
    return true;
  } catch (e) {
    // Declare that is not a standard fullscreen plugin wrapper for any error
    return false;
  }
}

function isScriptAllowed(allowScriptAccessParameter, url, pageUrl) {
  if (!allowScriptAccessParameter) {
    allowScriptAccessParameter = 'sameDomain';
  }
  var allowScriptAccess = false;
  switch (allowScriptAccessParameter.toLowerCase()) { // ignoring case here
    case 'always':
      allowScriptAccess = true;
      break;
    case 'never':
      allowScriptAccess = false;
      break;
    default: // 'samedomain'
      if (!pageUrl)
        break;
      try {
        // checking if page is in same domain (? same protocol and port)
        allowScriptAccess =
          Services.io.newURI('/', null, Services.io.newURI(pageUrl, null, null)).spec ==
          Services.io.newURI('/', null, Services.io.newURI(url, null, null)).spec;
      } catch (ex) {}
      break;
  }
  return allowScriptAccess;
}

function getStartupInfo(element) {
  var initStartTime = Date.now();
  var baseUrl;
  var pageUrl;
  var isOverlay = false;
  var objectParams = {};

  // Getting absolute URL from the EMBED tag
  var url = element.srcURI && element.srcURI.spec;

  pageUrl = element.ownerDocument.location.href; // proper page url?

  var tagName = element.nodeName;
  if (tagName == 'EMBED') {
    for (var i = 0; i < element.attributes.length; ++i) {
      var paramName = element.attributes[i].localName.toLowerCase();
      objectParams[paramName] = element.attributes[i].value;
    }
  } else {
    for (var i = 0; i < element.childNodes.length; ++i) {
      var paramElement = element.childNodes[i];
      if (paramElement.nodeType != 1 ||
        paramElement.nodeName != 'PARAM') {
        continue;
      }
      var paramName = paramElement.getAttribute('name').toLowerCase();
      objectParams[paramName] = paramElement.getAttribute('value');
    }
  }

  baseUrl = pageUrl;
  if (objectParams.base) {
    try {
      // Verifying base URL, passed in object parameters. It shall be okay to
      // ignore bad/corrupted base.
      var parsedPageUrl = Services.io.newURI(pageUrl, null, null);
      baseUrl = Services.io.newURI(objectParams.base, null, parsedPageUrl).spec;
    } catch (e) { /* it's okay to ignore any exception */ }
  }

  var movieParams = {};
  if (objectParams.flashvars) {
    movieParams = parseQueryString(objectParams.flashvars);
  }
  var queryStringMatch = url && /\?([^#]+)/.exec(url);
  if (queryStringMatch) {
    var queryStringParams = parseQueryString(queryStringMatch[1]);
    for (var i in queryStringParams) {
      if (!(i in movieParams)) {
        movieParams[i] = queryStringParams[i];
      }
    }
  }

  var allowScriptAccess = !!url &&
    isScriptAllowed(objectParams.allowscriptaccess, url, pageUrl);
  var isFullscreenSwf = isStandardEmbedWrapper(element);

  var document = element.ownerDocument;
  var window = document.defaultView;

  var startupInfo = {};
  startupInfo.window = window;
  startupInfo.url = url;
  startupInfo.privateBrowsing = isContentWindowPrivate(window);
  startupInfo.objectParams = objectParams;
  startupInfo.movieParams = movieParams;
  startupInfo.baseUrl = baseUrl || url;
  startupInfo.isOverlay = isOverlay;
  startupInfo.refererUrl = !isFullscreenSwf ? baseUrl : null;
  startupInfo.embedTag = element;
  startupInfo.initStartTime = initStartTime;
  startupInfo.allowScriptAccess = allowScriptAccess;
  startupInfo.pageIndex = 0;
  return startupInfo;
}
