/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci, Cc } = require("chrome");
var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn, dumpv } = DevToolsUtils;
loader.lazyRequireGetter(this, "prompt", "devtools/shared/security/prompt");
loader.lazyRequireGetter(this, "cert", "devtools/shared/security/cert");
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

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
   * identify the client in the future, such as the cert used during OOB_CERT.
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
   * @param cert object (optional)
   *        The server's cert details.
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
   * Verify that listener settings are appropriate for this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener about to be opened.
   * @throws if validation requirements are not met
   */
  validateOptions() {},

  /**
   * Augment options on the listening socket about to be opened.
   *
   * @param listener SocketListener
   *        The socket listener about to be opened.
   * @param socket nsIServerSocket
   *        The socket that is about to start listening.
   */
  augmentSocketOptions() {},

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

/**
 * The out-of-band (OOB) cert authenticator is based on self-signed X.509 certs
 * at both the client and server end.
 *
 * The user is first prompted to verify the connection, similar to the prompt
 * method above.  This prompt may display cert fingerprints if desired.
 *
 * Assuming the user approves the connection, further UI is used to assist the
 * user in tranferring out-of-band (OOB) verification of the client's
 * certificate.  For example, this could take the form of a QR code that the
 * client displays which is then scanned by a camera on the server.
 *
 * Since it is assumed that an attacker can't forge the client's X.509 cert, the
 * user may also choose to always allow a client, which would permit immediate
 * connections in the future with no user interaction needed.
 *
 * See docs/wifi.md for details of the authentication design.
 */
var OOBCert = (Authenticators.OOBCert = {});

OOBCert.mode = "OOB_CERT";

OOBCert.Client = function() {};
OOBCert.Client.prototype = {
  mode: OOBCert.mode,

  /**
   * When client is about to make a new connection, verify that the connection settings
   * are compatible with this authenticator.
   * @throws if validation requirements are not met
   */
  validateSettings({ encryption }) {
    if (!encryption) {
      throw new Error(`${OOBCert.mode} authentication requires encryption.`);
    }
  },

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
   * @param cert object (optional)
   *        The server's cert details.
   * @param socket nsISocketTransport
   *        Underlying socket transport, in case more details are needed.
   * @return boolean
   *         Whether the connection is valid.
   */
  // eslint-disable-next-line no-shadow
  validateConnection({ cert, socket }) {
    // Step B.7
    // Client verifies that Server's cert matches hash(ServerCert) from the
    // advertisement
    dumpv("Validate server cert hash");
    const serverCert = socket.securityInfo.QueryInterface(
      Ci.nsITransportSecurityInfo
    ).serverCert;
    const advertisedCert = cert;
    if (serverCert.sha256Fingerprint != advertisedCert.sha256) {
      dumpn("Server cert hash doesn't match advertisement");
      return false;
    }
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
   * @param cert object (optional)
   *        The server's cert details.
   * @param transport DebuggerTransport
   *        A transport that can be used to communicate with the server.
   * @return A promise can be used if there is async behavior.
   */
  // eslint-disable-next-line no-shadow
  authenticate({ host, port, cert, transport }) {
    return new Promise((resolve, reject) => {
      let oobData;

      let activeSendDialog;
      const closeDialog = () => {
        // Close any prompts the client may have been showing from previous
        // authentication steps
        if (activeSendDialog?.close) {
          activeSendDialog.close();
          activeSendDialog = null;
        }
      };

      transport.hooks = {
        onPacket: async packet => {
          closeDialog();
          const { authResult } = packet;
          switch (authResult) {
            case AuthenticationResult.PENDING:
              // Step B.8
              // Client creates hash(ClientCert) + K(random 128-bit number)
              oobData = await this._createOOB();
              activeSendDialog = this.sendOOB({
                host,
                port,
                cert,
                authResult,
                oob: oobData,
              });
              break;
            case AuthenticationResult.ALLOW:
              // Step B.12
              // Client verifies received value matches K
              if (packet.k != oobData.k) {
                transport.close(new Error("Auth secret mismatch"));
                return;
              }
              // Step B.13
              // Debugging begins
              transport.hooks = null;
              resolve(transport);
              break;
            case AuthenticationResult.ALLOW_PERSIST:
              // Server previously persisted Client as allowed
              // Step C.5
              // Debugging begins
              transport.hooks = null;
              resolve(transport);
              break;
            default:
              transport.close(new Error("Invalid auth result: " + authResult));
              break;
          }
        },
        onTransportClosed(reason) {
          closeDialog();
          // Transport died before auth completed
          transport.hooks = null;
          reject(reason);
        },
      };
      transport.ready();
    });
  },

  /**
   * Create the package of data that needs to be transferred across the OOB
   * channel.
   */
  async _createOOB() {
    const clientCert = await cert.local.getOrCreate();
    return {
      sha256: clientCert.sha256Fingerprint,
      k: this._createRandom(),
    };
  },

  _createRandom() {
    // 16 bytes / 128 bits
    const length = 16;
    const rng = Cc["@mozilla.org/security/random-generator;1"].createInstance(
      Ci.nsIRandomGenerator
    );
    const bytes = rng.generateRandomBytes(length);
    return bytes.map(byte => byte.toString(16)).join("");
  },

  /**
   * Send data across the OOB channel to the server to authenticate the devices.
   *
   * @param host string
   *        The host name or IP address of the devtools server.
   * @param port number
   *        The port number of the devtools server.
   * @param cert object (optional)
   *        The server's cert details.
   * @param authResult AuthenticationResult
   *        Authentication result sent from the server.
   * @param oob object (optional)
   *        The token data to be transferred during OOB_CERT step 8:
   *        * sha256: hash(ClientCert)
   *        * k     : K(random 128-bit number)
   * @return object containing:
   *         * close: Function to hide the notification
   */
  sendOOB: prompt.Client.defaultSendOOB,
};

OOBCert.Server = function() {};
OOBCert.Server.prototype = {
  mode: OOBCert.mode,

  /**
   * Verify that listener settings are appropriate for this authentication mode.
   *
   * @param listener SocketListener
   *        The socket listener about to be opened.
   * @throws if validation requirements are not met
   */
  validateOptions(listener) {
    if (!listener.encryption) {
      throw new Error(OOBCert.mode + " authentication requires encryption.");
    }
  },

  /**
   * Augment options on the listening socket about to be opened.
   *
   * @param listener SocketListener
   *        The socket listener about to be opened.
   * @param socket nsIServerSocket
   *        The socket that is about to start listening.
   */
  augmentSocketOptions(listener, socket) {
    const requestCert = Ci.nsITLSServerSocket.REQUIRE_ALWAYS;
    socket.setRequestClientCertificate(requestCert);
  },

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
    advertisement.authentication = OOBCert.mode;
    // Step A.4
    // Server announces itself via service discovery
    // Announcement contains hash(ServerCert) as additional data
    advertisement.cert = listener.cert;
  },

  /**
   * Determine whether a connection the server should be allowed or not based on
   * this authenticator's policies.
   *
   * @param session object
   *        In OOB_CERT mode, the |session| includes:
   *        {
   *          client: {
   *            host,
   *            port,
   *            cert: {
   *              sha256
   *            },
   *          },
   *          server: {
   *            host,
   *            port,
   *            cert: {
   *              sha256
   *            }
   *          },
   *          transport
   *        }
   * @return An AuthenticationResult value.
   *         A promise that will be resolved to the above is also allowed.
   */
  async authenticate({ client, server, transport }) {
    // Step B.3 / C.3
    // TLS connection established, authentication begins
    const storageKey = `devtools.auth.${this.mode}.approved-clients`;
    const approvedClients = (await asyncStorage.getItem(storageKey)) || {};
    // Step C.4
    // Server sees that ClientCert is from a known client via hash(ClientCert)
    if (approvedClients[client.cert.sha256]) {
      const authResult = AuthenticationResult.ALLOW_PERSIST;
      transport.send({ authResult });
      // Step C.5
      // Debugging begins
      return authResult;
    }

    // Step B.4
    // Server sees that ClientCert is from a unknown client
    // Tell client they are unknown and should display OOB client UX
    transport.send({
      authResult: AuthenticationResult.PENDING,
    });

    // Step B.5
    // User is shown a Allow / Deny / Always Allow prompt on the Server
    // with Client name and hash(ClientCert)
    const authResult = await this.allowConnection({
      authentication: this.mode,
      client,
      server,
    });

    switch (authResult) {
      case AuthenticationResult.ALLOW_PERSIST:
      case AuthenticationResult.ALLOW:
        // Further processing
        break;
      default:
        // Abort for any negative results
        return authResult;
    }

    // Examine additional data for authentication
    const oob = await this.receiveOOB();
    if (!oob) {
      dumpn("Invalid OOB data received");
      return AuthenticationResult.DENY;
    }

    const { sha256, k } = oob;
    // The OOB auth prompt should have transferred:
    // hash(ClientCert) + K(random 128-bit number)
    // from the client.
    if (!sha256 || !k) {
      dumpn("Invalid OOB data received");
      return AuthenticationResult.DENY;
    }

    // Step B.10
    // Server verifies that Client's cert matches hash(ClientCert) from
    // out-of-band channel
    if (client.cert.sha256 != sha256) {
      dumpn("Client cert hash doesn't match OOB data");
      return AuthenticationResult.DENY;
    }

    // Step B.11
    // Server sends K to Client over TLS connection
    transport.send({ authResult, k });

    // Persist Client if we want to always allow in the future
    if (authResult === AuthenticationResult.ALLOW_PERSIST) {
      approvedClients[client.cert.sha256] = true;
      await asyncStorage.setItem(storageKey, approvedClients);
    }

    // Client may decide to abort if K does not match.
    // Server's portion of authentication is now complete.

    // Step B.13
    // Debugging begins
    return authResult;
  },

  /**
   * Prompt the user to accept or decline the incoming connection. The default
   * implementation is used unless this is overridden on a particular
   * authenticator instance.
   *
   * It is expected that the implementation of |allowConnection| will show a
   * prompt to the user so that they can allow or deny the connection.
   *
   * @param session object
   *        In OOB_CERT mode, the |session| includes:
   *        {
   *          authentication: "OOB_CERT",
   *          client: {
   *            host,
   *            port,
   *            cert: {
   *              sha256
   *            },
   *          },
   *          server: {
   *            host,
   *            port,
   *            cert: {
   *              sha256
   *            }
   *          }
   *        }
   * @return An AuthenticationResult value.
   *         A promise that will be resolved to the above is also allowed.
   */
  allowConnection: prompt.Server.defaultAllowConnection,

  /**
   * The user must transfer some data through some out of band mechanism from
   * the client to the server to authenticate the devices.
   *
   * @return An object containing:
   *         * sha256: hash(ClientCert)
   *         * k     : K(random 128-bit number)
   *         A promise that will be resolved to the above is also allowed.
   */
  receiveOOB: prompt.Server.defaultReceiveOOB,
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
