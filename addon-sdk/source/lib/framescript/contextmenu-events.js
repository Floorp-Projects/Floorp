/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Holds remote items for this frame.
let keepAlive = new Map();

// Called to create remote proxies for items. If they already exist we destroy
// and recreate. This cna happen if the item changes in some way or in odd
// timing cases where the frame script is create around the same time as the
// item is created in the main process
addMessageListener('sdk/contextmenu/createitems', ({ data: { items, addon }}) => {
  let { loader } = Cu.import(addon.paths[''] + 'framescript/LoaderHelper.jsm', {});

  for (let itemoptions of items) {
    let { RemoteItem } = loader(addon).require('sdk/content/context-menu');
    let item = new RemoteItem(itemoptions, this);

    let oldItem = keepAlive.get(item.id);
    if (oldItem) {
      oldItem.destroy();
    }

    keepAlive.set(item.id, item);
  }
});

addMessageListener('sdk/contextmenu/destroyitems', ({ data: { items }}) => {
  for (let id of items) {
    let item = keepAlive.get(id);
    item.destroy();
    keepAlive.delete(id);
  }
});

sendAsyncMessage('sdk/contextmenu/requestitems');

Services.obs.addObserver(function(subject, topic, data) {
  // Many frame scripts run in the same process, check that the context menu
  // node is in this frame
  let { event: { target: popupNode }, addonInfo } = subject.wrappedJSObject;
  if (popupNode.ownerDocument.defaultView.top != content)
    return;

  for (let item of keepAlive.values()) {
    item.getContextState(popupNode, addonInfo);
  }
}, "content-contextmenu", false);

addMessageListener('sdk/contextmenu/activateitems', ({ data: { items, data }, objects: { popupNode }}) => {
  for (let id of items) {
    let item = keepAlive.get(id);
    if (!item)
      continue;

    item.activate(popupNode, data);
  }
});
