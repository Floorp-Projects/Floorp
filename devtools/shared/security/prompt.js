/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Ci } = require("chrome");
var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "AuthenticationResult",
  "devtools/shared/security/auth", true);

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/shared/locales/debugger.properties");

var Client = exports.Client = {};
var Server = exports.Server = {};

/**
 * During OOB_CERT authentication, a notification dialog like this is used to
 * to display a token which the user must transfer through some mechanism to the
 * server to authenticate the devices.
 *
 * This implementation presents the token as text for the user to transfer
 * manually.  For a mobile device, you should override this implementation with
 * something more convenient, such as displaying a QR code.
 *
 * @param host string
 *        The host name or IP address of the debugger server.
 * @param port number
 *        The port number of the debugger server.
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
Client.defaultSendOOB = ({ authResult, oob }) => {
  // Only show in the PENDING state
  if (authResult != AuthenticationResult.PENDING) {
    throw new Error("Expected PENDING result, got " + authResult);
  }
  const title = L10N.getStr("clientSendOOBTitle");
  const header = L10N.getStr("clientSendOOBHeader");
  const hashMsg = L10N.getFormatStr("clientSendOOBHash", oob.sha256);
  const token = oob.sha256.replace(/:/g, "").toLowerCase() + oob.k;
  const tokenMsg = L10N.getFormatStr("clientSendOOBToken", token);
  const msg = `${header}\n\n${hashMsg}\n${tokenMsg}`;
  const prompt = Services.prompt;
  const flags = prompt.BUTTON_POS_0 * prompt.BUTTON_TITLE_CANCEL;

  // Listen for the window our prompt opens, so we can close it programatically
  let promptWindow;
  const windowListener = {
    onOpenWindow(xulWindow) {
      const win = xulWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindow);
      win.addEventListener("load", function() {
        if (win.document.documentElement.getAttribute("id") != "commonDialog") {
          return;
        }
        // Found the window
        promptWindow = win;
        Services.wm.removeListener(windowListener);
      }, {once: true});
    },
    onCloseWindow() {},
  };
  Services.wm.addListener(windowListener);

  // nsIPrompt is typically a blocking API, so |executeSoon| to get around this
  DevToolsUtils.executeSoon(() => {
    prompt.confirmEx(null, title, msg, flags, null, null, null, null,
                     { value: false });
  });

  return {
    close() {
      if (!promptWindow) {
        return;
      }
      promptWindow.document.documentElement.acceptDialog();
      promptWindow = null;
    }
  };
};

/**
 * Prompt the user to accept or decline the incoming connection.  This is the
 * default implementation that products embedding the debugger server may
 * choose to override.  This can be overridden via |allowConnection| on the
 * socket's authenticator instance.
 *
 * @param session object
 *        The session object will contain at least the following fields:
 *        {
 *          authentication,
 *          client: {
 *            host,
 *            port
 *          },
 *          server: {
 *            host,
 *            port
 *          }
 *        }
 *        Specific authentication modes may include additional fields.  Check
 *        the different |allowConnection| methods in ./auth.js.
 * @return An AuthenticationResult value.
 *         A promise that will be resolved to the above is also allowed.
 */
Server.defaultAllowConnection = ({ client, server }) => {
  const title = L10N.getStr("remoteIncomingPromptTitle");
  const header = L10N.getStr("remoteIncomingPromptHeader");
  const clientEndpoint = `${client.host}:${client.port}`;
  const clientMsg =
    L10N.getFormatStr("remoteIncomingPromptClientEndpoint", clientEndpoint);
  const serverEndpoint = `${server.host}:${server.port}`;
  const serverMsg =
    L10N.getFormatStr("remoteIncomingPromptServerEndpoint", serverEndpoint);
  const footer = L10N.getStr("remoteIncomingPromptFooter");
  const msg = `${header}\n\n${clientMsg}\n${serverMsg}\n\n${footer}`;
  const disableButton = L10N.getStr("remoteIncomingPromptDisable");
  const prompt = Services.prompt;
  const flags = prompt.BUTTON_POS_0 * prompt.BUTTON_TITLE_OK +
              prompt.BUTTON_POS_1 * prompt.BUTTON_TITLE_CANCEL +
              prompt.BUTTON_POS_2 * prompt.BUTTON_TITLE_IS_STRING +
              prompt.BUTTON_POS_1_DEFAULT;
  const result = prompt.confirmEx(null, title, msg, flags, null, null,
                                disableButton, null, { value: false });
  if (result === 0) {
    return AuthenticationResult.ALLOW;
  }
  if (result === 2) {
    return AuthenticationResult.DISABLE_ALL;
  }
  return AuthenticationResult.DENY;
};

/**
 * During OOB_CERT authentication, the user must transfer some data through some
 * out of band mechanism from the client to the server to authenticate the
 * devices.
 *
 * This implementation prompts the user for a token as constructed by
 * |Client.defaultSendOOB| that the user needs to transfer manually.  For a
 * mobile device, you should override this implementation with something more
 * convenient, such as reading a QR code.
 *
 * @return An object containing:
 *         * sha256: hash(ClientCert)
 *         * k     : K(random 128-bit number)
 *         A promise that will be resolved to the above is also allowed.
 */
Server.defaultReceiveOOB = () => {
  const title = L10N.getStr("serverReceiveOOBTitle");
  const msg = L10N.getStr("serverReceiveOOBBody");
  let input = { value: null };
  const prompt = Services.prompt;
  const result = prompt.prompt(null, title, msg, input, null, { value: false });
  if (!result) {
    return null;
  }
  // Re-create original object from token
  input = input.value.trim();
  let sha256 = input.substring(0, 64);
  sha256 = sha256.replace(/\w{2}/g, "$&:").slice(0, -1).toUpperCase();
  const k = input.substring(64);
  return { sha256, k };
};
