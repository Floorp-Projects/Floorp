/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { networkContentSpec } = require("devtools/shared/specs/network-content");

const { Cc, Ci } = require("chrome");

loader.lazyRequireGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm",
  true
);

loader.lazyRequireGetter(
  this,
  "NetworkUtils",
  "devtools/server/actors/network-monitor/utils/network-utils"
);

loader.lazyRequireGetter(
  this,
  "WebConsoleUtils",
  "devtools/server/actors/webconsole/utils",
  true
);

const {
  TYPES: { NETWORK_EVENT_STACKTRACE },
  getResourceWatcher,
} = require("devtools/server/actors/resources/index");

/**
 * This actor manages all network functionality runnning
 * in the content process.
 *
 * @constructor
 *
 */
const NetworkContentActor = ActorClassWithSpec(networkContentSpec, {
  initialize(conn, targetActor) {
    Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;
  },

  destroy(conn) {
    Actor.prototype.destroy.call(this, conn);
  },

  get networkEventStackTraceWatcher() {
    return getResourceWatcher(this.targetActor, NETWORK_EVENT_STACKTRACE);
  },

  /**
   *  Send an HTTP request
   *
   * @param {Object} request
   *        The details of the HTTP Request.
   * @return {Number}
   *        The channel id for the request
   */
  async sendHTTPRequest(request) {
    const { url, method, headers, body, cause } = request;
    // Set the loadingNode and loadGroup to the target document - otherwise the
    // request won't show up in the opened netmonitor.
    const doc = this.targetActor.window.document;

    const channel = NetUtil.newChannel({
      uri: NetUtil.newURI(url),
      loadingNode: doc,
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      contentPolicyType:
        NetworkUtils.stringToCauseType(cause.type) ||
        Ci.nsIContentPolicy.TYPE_OTHER,
    });

    channel.QueryInterface(Ci.nsIHttpChannel);

    channel.loadGroup = doc.documentLoadGroup;
    channel.loadFlags |=
      Ci.nsIRequest.LOAD_BYPASS_CACHE |
      Ci.nsIRequest.INHIBIT_CACHING |
      Ci.nsIRequest.LOAD_ANONYMOUS;

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

    return new Promise(resolve => {
      // Make sure the fetch has completed before sending the channel id,
      // so that there is a higher possibilty that the request get into the
      // redux store beforehand (but this does not gurantee that).
      NetUtil.asyncFetch(channel, () =>
        resolve({ channelId: channel.channelId })
      );
    });
  },

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
    const stacktrace = this.networkEventStackTraceWatcher.getStackTrace(
      resourceId
    );
    return WebConsoleUtils.removeFramesAboveDebuggerEval(stacktrace);
  },
});

exports.NetworkContentActor = NetworkContentActor;
