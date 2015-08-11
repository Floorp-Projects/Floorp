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

var EXPORTED_SYMBOLS = ['LocalConnectionService'];

Components.utils.import('resource://gre/modules/NetUtil.jsm');
Components.utils.import('resource://gre/modules/Services.jsm');

const localConnectionsRegistry = Object.create(null);

function isConnectionNameValid(connectionName) {
  return typeof connectionName === 'string' &&
         (connectionName[0] === '_' || connectionName.split(':').length === 2);
}

/**
 * Creates a trusted qualified connection name from an already qualified name and a swfUrl.
 *
 * While connection names are already qualified at this point, the qualification happens in
 * untrusted code. To ensure that the name is correctly qualified, this function compares the
 * qualification domain with the current SWF's URL and substitutes that URL's domain if
 * required. A warning is logged in that case.
 */
function _getQualifiedConnectionName(connectionName, swfUrl) {
  // Already syntactically invalid connection names mustn't get here.
  if (!isConnectionNameValid(connectionName)) {
    // TODO: add telemetry
    throw new Error('Syntactically invalid local-connection name encountered', connectionName,
                    swfUrl);
  }
  var [domain, name] = connectionName.split(':');
  var parsedURL = NetUtil.newURI(swfUrl);
  if (domain !== parsedURL.host) {
    // TODO: add telemetry
    log('Warning: invalid local-connection name qualification found: ' + connectionName);
    return parsedURL.host + ':' + swfUrl;
  }
  return connectionName;
}

function _getLocalConnection(connectionName) {
  // Treat invalid connection names as non-existent. This can only happen if player code
  // misbehaves, though.
  if (!isConnectionNameValid(connectionName)) {
    // TODO: add telemetry
    return null;
  }
  var connection = localConnectionsRegistry[connectionName];
  if (connection && Components.utils.isDeadWrapper(connection.callback)) {
    delete localConnectionsRegistry[connectionName];
    return null;
  }
  return localConnectionsRegistry[connectionName];
}

function LocalConnectionService(content, environment) {
  var traceLocalConnection = getBoolPref('shumway.localConnection.trace', false);
  var api = {
    createLocalConnection: function (connectionName, callback) {
      connectionName = connectionName + '';
      traceLocalConnection && content.console.log(`Creating local connection "${connectionName}" ` +
                                                  `for SWF with URL ${environment.swfUrl}`);

      if (!isConnectionNameValid(connectionName)) {
        // TODO: add telemetry
        traceLocalConnection && content.console.warn(`Invalid localConnection name `);
        return -1; // LocalConnectionConnectResult.InvalidName
      }
      if (typeof callback !== 'function') {
        // TODO: add telemetry
        traceLocalConnection && content.console.warn(`Invalid callback for localConnection`);
        return -3; // LocalConnectionConnectResult.InvalidCallback
      }
      connectionName = _getQualifiedConnectionName(connectionName, environment.swfUrl);
      if (_getLocalConnection(connectionName)) {
        traceLocalConnection && content.console.log(`localConnection ` +
                                                     `name "${connectionName}" already taken`);
        return -2; // LocalConnectionConnectResult.AlreadyTaken
      }

      var parsedURL = NetUtil.newURI(environment.swfUrl);

      var connection = {
        callback,
        domain: parsedURL.host,
        environment: environment,
        secure: parsedURL.protocol === 'https:',
        allowedSecureDomains: Object.create(null),
        allowedInsecureDomains: Object.create(null)
      };
      localConnectionsRegistry[connectionName] = connection;
      return 0; // LocalConnectionConnectResult.Success
    },
    hasLocalConnection: function (connectionName) {
      connectionName = _getQualifiedConnectionName(connectionName + '', environment.swfUrl);

      var result = !!_getLocalConnection(connectionName);
      traceLocalConnection && content.console.log(`hasLocalConnection "${connectionName}"? ` +
                                                  result);
      return result;
    },
    closeLocalConnection: function (connectionName) {
      connectionName = _getQualifiedConnectionName(connectionName + '', environment.swfUrl);
      traceLocalConnection && content.console.log(`Closing local connection "${connectionName}" ` +
                                                  `for SWF with URL ${environment.swfUrl}`);

      var connection = _getLocalConnection(connectionName);
      if (!connection) {
        traceLocalConnection && content.console.log(`localConnection "${connectionName}" not ` +
                                                    `connected`);
        return -1; // LocalConnectionCloseResult.NotConnected
      } else if (connection.environment !== environment) {
        // Attempts to close connections from a SWF instance that didn't create them shouldn't
        // happen. If they do, we treat them as if the connection didn't exist.
        traceLocalConnection && content.console.warn(`Ignored attempt to close localConnection ` +
                                                     `"${connectionName}" from SWF instance that ` +
                                                     `didn't create it`);
        return -1; // LocalConnectionCloseResult.NotConnected
      }
      delete localConnectionsRegistry[connectionName];
      return 0; // LocalConnectionCloseResult.Success
    },
    sendLocalConnectionMessage: function (connectionName, methodName, argsBuffer, sender,
                                          senderDomain, senderIsSecure) {
      connectionName = connectionName + '';
      methodName = methodName + '';
      senderDomain = senderDomain + '';
      senderIsSecure = !!senderIsSecure;
      // TODO: sanitize argsBuffer argument. Ask bholley how to do so.
      traceLocalConnection && content.console.log(`sending localConnection message ` +
                                                  `"${methodName}" to "${connectionName}"`);

      // Since we don't currently trust the sender information passed in here, we use the
      // currently running SWF's URL instead.
      var parsedURL = NetUtil.newURI(environment.swfUrl);
      var parsedURLIsSecure = parsedURL.protocol === 'https:';
      if (parsedURL.host !== senderDomain || parsedURLIsSecure !== senderIsSecure) {
        traceLocalConnection && content.console.warn(`sending localConnection message ` +
                                                     `"${methodName}" to "${connectionName}"`);
      }
      senderDomain = parsedURL.host;
      senderIsSecure = parsedURLIsSecure;

      var connection = _getLocalConnection(connectionName);
      if (!connection) {
        traceLocalConnection && content.console.log(`localConnection "${connectionName}" not ` +
                                                    `connected`);
        return;
      }
      try {
        var allowed = false;
        if (connection.secure) {
          // If the receiver is secure, the sender has to be, too, or it has to be whitelisted
          // with allowInsecureDomain.
          if (senderIsSecure) {
            if (senderDomain === connection.domain ||
                senderDomain in connection.allowedSecureDomains ||
                '*' in connection.allowedSecureDomains) {
              allowed = true;
            }
          } else {
            if (senderDomain in connection.allowedInsecureDomains ||
                '*' in connection.allowedInsecureDomains) {
              allowed = true;
            }
          }
        } else {
          // For non-secure connections, allowedSecureDomains is expected to contain all allowed
          // domains, secure on non-secure, so we don't have to check both.
          if (senderDomain === connection.domain ||
              senderDomain in connection.allowedSecureDomains ||
              '*' in connection.allowedSecureDomains) {
            allowed = true;
          }
        }
        if (!allowed) {
          traceLocalConnection && content.console.warn(`LocalConnection message rejected: domain ` +
                                                       `${senderDomain} not allowed.`);
          return {
            name: 'SecurityError',
            $Bgmessage: "The current security context does not allow this operation.",
            _errorID: 3315
          };
        }
        var callback = connection.callback;
        var clonedArgs = Components.utils.cloneInto(argsBuffer, callback);
        callback(methodName, clonedArgs);
      } catch (e) {
        // TODO: add telemetry
        content.console.warn('Unexpected error encountered while sending LocalConnection message.');
      }
    },
    allowDomainsForLocalConnection: function (connectionName, domains, secure) {
      connectionName = _getQualifiedConnectionName(connectionName + '', environment.swfUrl);
      secure = !!secure;
      var connection = _getLocalConnection(connectionName);
      if (!connection) {
        return;
      }
      try {
        domains = Components.utils.cloneInto(domains, connection);
      } catch (e) {
        log('error in allowDomainsForLocalConnection: ' + e);
        return;
      }
      traceLocalConnection && content.console.log(`allowing ${secure ? '' : 'in'}secure domains ` +
                                                  `[${domains}] for localConnection ` +
                                                  `"${connectionName}"`);
      function validateDomain(domain) {
        if (typeof domain !== 'string') {
          return false;
        }
        if (domain === '*') {
          return true;
        }
        try {
          var uri = NetUtil.newURI('http://' + domain);
          return uri.host === domain;
        } catch (e) {
          return false;
        }
      }
      if (!Array.isArray(domains) || !domains.every(validateDomain)) {
        traceLocalConnection && content.console.warn(`Invalid domains rejected`);
        return;
      }
      var allowedDomains = secure ?
                           connection.allowedSecureDomains :
                           connection.allowedInsecureDomains;
      domains.forEach(domain => allowedDomains[domain] = true);
    }
  };

  // Don't return `this` even though this function is treated as a ctor. Makes cloning into the
  // content compartment an internal operation the client code doesn't have to worry about.
  return Components.utils.cloneInto(api, content, {cloneFunctions:true});
}

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}
