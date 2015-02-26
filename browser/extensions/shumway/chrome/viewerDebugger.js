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

Components.utils.import('resource://gre/modules/Services.jsm');

var DebugUtils = (function () {
  var baseUrl = null;

  function getBaseUrl() {
    if (baseUrl === null) {
      try {
        baseUrl = Services.prefs.getCharPref('shumway.debug.url');
      } catch (e) {
        baseUrl = 'http://localhost:8010';
      }
    }
    return baseUrl;
  }

  var uniqueId = (Date.now() % 888888) * 2 + 1;

  function getEnabledDebuggerId(swfUrl) {
    return new Promise(function (resolve) {
      var url = getBaseUrl() + '/debugController/' + uniqueId;
      var connection = new PingPongConnection(url);
      connection.onData = function (data) {
        if (data.action === 'setDebugger' && data.swfUrl === swfUrl) {
          resolve(data.debuggerId);
        }
      };
      try {
        connection.send({action: 'getDebugger', swfUrl: swfUrl, swfId: uniqueId}, true);
      } catch (e) {
        // ignoring failed send request
      }
      setTimeout(function () {
        resolve(0);
        connection.close();
      }, 500);
    });
  }

  function enableDebug(swfUrl) {
    var url = getBaseUrl() + '/debugController/' + uniqueId;
    var connection = new PingPongConnection(url, true);
    try {
      connection.send({action: 'enableDebugging', swfUrl: swfUrl}, true);
    } catch (e) {
      // ignoring failed send request
    }
    connection.close();
  }

  function createDebuggerConnection(swfUrl) {
    return getEnabledDebuggerId(swfUrl).then(function (debuggerId) {
      if (!debuggerId) {
        return null;
      }

      var url = getBaseUrl() + '/debug/' + uniqueId + '/' + debuggerId;
      console.log('Starting remote debugger with ' + url);
      return new PingPongConnection(url);
    });
  }

  return {
    get isEnabled() {
      try {
        return Services.prefs.getBoolPref('shumway.debug.enabled');
      } catch (e) {
        return false;
      }
    },
    enableDebug: enableDebug,
    createDebuggerConnection: createDebuggerConnection
  };
})();
