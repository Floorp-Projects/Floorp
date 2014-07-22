/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
(function({content, sendSyncMessage, addMessageListener, sendAsyncMessage}) {

const Cc = Components.classes;
const Ci = Components.interfaces;
const observerService = Cc["@mozilla.org/observer-service;1"]
                        .getService(Ci.nsIObserverService);

const channels = new Map();
const handles = new WeakMap();

// Takes remote port handle and creates a local one.
// also set's up a messaging channel between them.
// This is temporary workaround until Bug 914974 is fixed
// and port can be transfered through message manager.
const demarshal = (handle) => {
  if (handle.type === "MessagePort") {
    if (!channels.has(handle.id)) {
      const channel = new content.MessageChannel();
      channels.set(handle.id, channel);
      handles.set(channel.port1, handle);
      channel.port1.onmessage = onOutPort;
    }
    return channels.get(handle.id).port2;
  }
  return null;
};

const onOutPort = event => {
  const handle = handles.get(event.target);
  sendAsyncMessage("sdk/port/message", {
    port: handle,
    message: event.data
  });
};

const onInPort = ({data}) => {
  const channel = channels.get(data.port.id);
  if (channel)
    channel.port1.postMessage(data.message);
};

const onOutEvent = event =>
  sendSyncMessage("sdk/event/" + event.type,
                  { type: event.type,
                    data: event.data });

const onInMessage = (message) => {
  const {type, data, origin, bubbles, cancelable, ports} = message.data;

  const event = new content.MessageEvent(type, {
    bubbles: bubbles,
    cancelable: cancelable,
    data: data,
    origin: origin,
    target: content,
    source: content,
    ports: ports.map(demarshal)
  });
  content.dispatchEvent(event);
};

const onReady = event => {
  channels.clear();
};

addMessageListener("sdk/event/message", onInMessage);
addMessageListener("sdk/port/message", onInPort);

const observer = {
  observe: (document, topic, data) => {
    // When frame associated with message manager is removed from document `docShell`
    // is set to `null` but observer is still kept alive. At this point accesing
    // `content.document` throws "can't access dead object" exceptions. In order to
    // avoid leaking observer and logged errors observer is going to be removed when
    // `docShell` is set to `null`.
    if (!docShell) {
      observerService.removeObserver(observer, topic);
    }
    else if (document === content.document) {
      if (topic === "content-document-interactive") {
        sendAsyncMessage("sdk/event/ready", {
          type: "ready",
          readyState: document.readyState,
          uri: document.documentURI
        });
      }
      if (topic === "content-document-loaded") {
        sendAsyncMessage("sdk/event/load", {
          type: "load",
          readyState: document.readyState,
          uri: document.documentURI
        });
      }
      if (topic === "content-page-hidden") {
        channels.clear();
        sendAsyncMessage("sdk/event/unload", {
          type: "unload",
          readyState: "uninitialized",
          uri: document.documentURI
        });
      }
    }
  }
};

observerService.addObserver(observer, "content-document-interactive", false);
observerService.addObserver(observer, "content-document-loaded", false);
observerService.addObserver(observer, "content-page-hidden", false);

})(this);
