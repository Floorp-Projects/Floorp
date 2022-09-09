/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "prompt", "devtools/shared/security/prompt");

/**
 * A simple enum-like object with keys mirrored to values.
 * This makes comparison to a specfic value simpler without having to repeat and
 * mis-type the value.
 */
function createEnum(obj) {
  for (const key in obj) {
    obj[key] = key;
  }
  return obj;
}

/**
 * |allowConnection| implementations can return various values as their |result|
 * field to indicate what action to take.  By specifying these, we can
 * centralize the common actions available, while still allowing embedders to
 * present their UI in whatever way they choose.
 */
var AuthenticationResult = (exports.AuthenticationResult = createEnum({
  /**
   * Close all listening sockets, and disable them from opening again.
   */
  DISABLE_ALL: null,

  /**
   * Deny the current connection.
   */
  DENY: null,

  /**
   * Additional data needs to be exchanged before a result can be determined.
   */
  PENDING: null,

  /**
   * Allow the current connection.
   */
  ALLOW: null,

  /**
   * Allow the current connection, and persist this choice for future
   * connections from the same client.  This requires a trustable mechanism to
   * identify the client in the future.
   */
  ALLOW_PERSIST: null,
}));

/**
 * An |Authenticator| implements an authentication mechanism via various hooks
 * in the client and server debugger socket connection path (see socket.js).
 *
 * |Authenticator|s are stateless objects.  Each hook method is passed the state
 * it needs by the client / server code in socket.js.
 *
 * Separate instances of the |Authenticator| are created for each use (client
 * connection, server listener) in case some methods are customized by the
 * embedder for a given use case.
 */
var Authenticators = {};

/**
 * The Prompt authenticator displays a server-side user prompt that includes
 * connection details, and asks the user to verify the connection.  There are
 * no cryptographic properties at work here, so it is up to the user to be sure
 * that the client can be trusted.
 */
var Prompt = (Authenticators.Prompt = {});

Prompt.mode = "PROMPT";

Prompt.Client = function() {};
Prompt.Client.prototype = {
  mode: Prompt.mode,

  /**
   * When client is about to make a new connection, verify that the connection settings
   * are compatible with this authenticator.
   * @throws if validation requirements are not met
   */
  validateSettings() {},

  /**
   * When client has just made a new socket connection, validate the connection
   * to ensure it meets the authenticator's policies.
   *
   * @param host string
   *        The host name or IP address of the devtools server.
   * @param port number
   *        The port number of the devtools server.
   * @param encryption boolean (optional)
   *        Whether the server requires encryption.  Defaults to false.
   * @param s nsISocketTransport
   *        Underlying socket transport, in case more details are needed.
   * @return boolean
   *         Whether the connection is valid.
   */
  validateConnection() {
    return true;
  },

  /**
   * Work with the server to complete any additional steps required by this
   * authenticator's policies.
   *
   * Debugging commences after this hook completes successfully.
   *
   * @param host string
   *        The host name or IP address of the devtools server.
   * @param port number
   *        The port number of the devtools server.
   * @param encryption boolean (optional)
   *        Whether the server requires encryption.  Defaults to false.
   * @param transport DebuggerTransport
   *        A transport that can be used to communicate with the server.
   * @return A promise can be used if there is async behavior.
   */
  authenticate() {},
};

Prompt.Server = function() {};
Prompt.Server.prototype = {
  mode: Prompt.mode,

  /**
   * Augment the service discovery advertisement with any additional data needed
   * to support this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener that was just opened.
   * @param advertisement object
   *        The advertisement being built.
   */
  augmentAdvertisement(listener, advertisement) {
    advertisement.authentication = Prompt.mode;
  },

  /**
   * Determine whether a connection the server should be allowed or not based on
   * this authenticator's policies.
   *
   * @param session object
   *        In PROMPT mode, the |session| includes:
   *        {
   *          client: {
   *            host,
   *            port
   *          },
   *          server: {
   *            host,
   *            port
   *          },
   *          transport
   *        }
   * @return An AuthenticationResult value.
   *         A promise that will be resolved to the above is also allowed.
   */
  authenticate({ client, server }) {
    if (!Services.prefs.getBoolPref("devtools.debugger.prompt-connection")) {
      return AuthenticationResult.ALLOW;
    }
    return this.allowConnection({
      authentication: this.mode,
      client,
      server,
    });
  },

  /**
   * Prompt the user to accept or decline the incoming connection.  The default
   * implementation is used unless this is overridden on a particular
   * authenticator instance.
   *
   * It is expected that the implementation of |allowConnection| will show a
   * prompt to the user so that they can allow or deny the connection.
   *
   * @param session object
   *        In PROMPT mode, the |session| includes:
   *        {
   *          authentication: "PROMPT",
   *          client: {
   *            host,
   *            port
   *          },
   *          server: {
   *            host,
   *            port
   *          }
   *        }
   * @return An AuthenticationResult value.
   *         A promise that will be resolved to the above is also allowed.
   */
  allowConnection: prompt.Server.defaultAllowConnection,
};

exports.Authenticators = {
  get(mode) {
    if (!mode) {
      mode = Prompt.mode;
    }
    for (const key in Authenticators) {
      const auth = Authenticators[key];
      if (auth.mode === mode) {
        return auth;
      }
    }
    throw new Error("Unknown authenticator mode: " + mode);
  },
};
