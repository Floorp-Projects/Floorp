/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;
const { Services } = Cu.import('resource://gre/modules/Services.jsm');

const cpmm = Cc['@mozilla.org/childprocessmessagemanager;1'].
             getService(Ci.nsISyncMessageSender);

this.EXPORTED_SYMBOLS = ["registerContentFrame"];

// This may be an overriden version of the SDK so use the PATH as a key for the
// initial messages before we have a loaderID.
const PATH = __URI__.replace('framescript/content.jsm', '');

const { Loader } = Cu.import(PATH + 'toolkit/loader.js', {});

// one Loader instance per addon (per @loader/options to be precise)
var addons = new Map();

// Tell the parent that a new process is ready
cpmm.sendAsyncMessage('sdk/remote/process/start', {
  modulePath: PATH
});

// Load a child process module loader with the given loader options
cpmm.addMessageListener('sdk/remote/process/load', ({ data: { modulePath, loaderID, options, reason } }) => {
  if (modulePath != PATH)
    return;

  // During startup races can mean we get a second load message
  if (addons.has(loaderID))
    return;

  options.waiveInterposition = true;

  let loader = Loader.Loader(options);
  let addon = {
    loader,
    require: Loader.Require(loader, { id: 'LoaderHelper' }),
  }
  addons.set(loaderID, addon);

  cpmm.sendAsyncMessage('sdk/remote/process/attach', {
    loaderID,
    processID: Services.appinfo.processID,
    isRemote: Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  });

  addon.child = addon.require('sdk/remote/child');

  for (let contentFrame of frames.values())
    addon.child.registerContentFrame(contentFrame);
});

// Unload a child process loader
cpmm.addMessageListener('sdk/remote/process/unload', ({ data: { loaderID, reason } }) => {
  if (!addons.has(loaderID))
    return;

  let addon = addons.get(loaderID);
  Loader.unload(addon.loader, reason);

  // We want to drop the reference to the loader but never allow creating a new
  // loader with the same ID
  addons.set(loaderID, {});
})


var frames = new Set();

this.registerContentFrame = contentFrame => {
  contentFrame.addEventListener("unload", () => {
    unregisterContentFrame(contentFrame);
  }, false);

  frames.add(contentFrame);

  for (let addon of addons.values()) {
    if ("child" in addon)
      addon.child.registerContentFrame(contentFrame);
  }
};

function unregisterContentFrame(contentFrame) {
  frames.delete(contentFrame);

  for (let addon of addons.values()) {
    if ("child" in addon)
      addon.child.unregisterContentFrame(contentFrame);
  }
}
