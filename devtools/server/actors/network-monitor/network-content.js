/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  networkContentSpec,
} = require("resource://devtools/shared/specs/network-content.js");

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  NetworkUtils:
    "resource://devtools/shared/network-observer/NetworkUtils.sys.mjs",
});

loader.lazyRequireGetter(
  this,
  "WebConsoleUtils",
  "resource://devtools/server/actors/webconsole/utils.js",
  true
);

const {
  TYPES: { NETWORK_EVENT_STACKTRACE },
  getResourceWatcher,
} = require("resource://devtools/server/actors/resources/index.js");

/**
 * This actor manages all network functionality runnning
 * in the content process.
 *
 * @constructor
 *
 */
class NetworkContentActor extends Actor {
  constructor(conn, targetActor) {
    super(conn, networkContentSpec);
    this.targetActor = targetActor;
  }

  get networkEventStackTraceWatcher() {
    return getResourceWatcher(this.targetActor, NETWORK_EVENT_STACKTRACE);
  }

  /**
   *  Send an HTTP request
   *
   * @param {Object} request
   *        The details of the HTTP Request.
   * @return {Number}
   *        The channel id for the request
   */
  async sendHTTPRequest(request) {
    return new Promise(resolve => {
      const { url, method, headers, body, cause } = request;
      // Set the loadingNode and loadGroup to the target document - otherwise the
      // request won't show up in the opened netmonitor.
      const doc = this.targetActor.window.document;

      const channel = lazy.NetUtil.newChannel({
        uri: lazy.NetUtil.newURI(url),
        loadingNode: doc,
        securityFlags:
          Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
        contentPolicyType:
          lazy.NetworkUtils.stringToCauseType(cause.type) ||
          Ci.nsIContentPolicy.TYPE_OTHER,
      });

      channel.QueryInterface(Ci.nsIHttpChannel);
      channel.loadGroup = doc.documentLoadGroup;
      channel.loadFlags |=
        Ci.nsIRequest.LOAD_BYPASS_CACHE |
        Ci.nsIRequest.INHIBIT_CACHING |
        Ci.nsIRequest.LOAD_ANONYMOUS;

      if (method == "CONNECT") {
        throw new Error(
          "The CONNECT method is restricted and cannot be sent by devtools"
        );
      }
      channel.requestMethod = method;

      if (headers) {
        for (const { name, value } of headers) {
          if (name.toLowerCase() == "referer") {
            // The referer header and referrerInfo object should always match. So
            // if we want to set the header from privileged context, we should set
            // referrerInfo. The referrer header will get set internally.
            channel.setNewReferrerInfo(
              value,
              Ci.nsIReferrerInfo.UNSAFE_URL,
              true
            );
          } else {
            channel.setRequestHeader(name, value, false);
          }
        }
      }

      if (body) {
        channel.QueryInterface(Ci.nsIUploadChannel2);
        const bodyStream = Cc[
          "@mozilla.org/io/string-input-stream;1"
        ].createInstance(Ci.nsIStringInputStream);
        bodyStream.setData(body, body.length);
        channel.explicitSetUploadStream(bodyStream, null, -1, method, false);
      }

      // Make sure the fetch has completed before sending the channel id,
      // so that there is a higher possibilty that the request get into the
      // redux store beforehand (but this does not gurantee that).
      lazy.NetUtil.asyncFetch(channel, () =>
        resolve({ channelId: channel.channelId })
      );
    });
  }

  /**
   * Gets the stacktrace for the specified network resource.
   *  @param {Number} resourceId
   *         The id for the network resource
   * @return {Object}
   *         The response packet - stack trace.
   */
  getStackTrace(resourceId) {
    if (!this.networkEventStackTraceWatcher) {
      throw new Error("Not listening for network event stacktraces");
    }
    const stacktrace =
      this.networkEventStackTraceWatcher.getStackTrace(resourceId);
    return WebConsoleUtils.removeFramesAboveDebuggerEval(stacktrace);
  }
}

exports.NetworkContentActor = NetworkContentActor;
