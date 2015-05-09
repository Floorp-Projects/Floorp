/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Ci = Components.interfaces;

addMessageListener("sdk/test/context-menu/open", message => {
  const {data, name} = message;
  const target = data.target && content.document.querySelector(data.target);
  if (target) {
    target.scrollIntoView();
  }
  const rect = target ? target.getBoundingClientRect() :
               {left:0, top:0, width:0, height:0};


  content.
    QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIDOMWindowUtils).
    sendMouseEvent("contextmenu",
                   rect.left + (rect.width / 2),
                   rect.top + (rect.height / 2),
                   2, 1, 0);
});

addMessageListener("sdk/test/context-menu/select", message => {
  const {data, name} = message;
  const {document} = content;
  if (data) {
    if (typeof(data) === "string") {
      const target = document.querySelector(data);
      document.getSelection().selectAllChildren(target);
    } else {
      const target = document.querySelector(data.target);
      target.focus();
      target.setSelectionRange(data.start, data.end);
    }
  } else {
    document.getSelection().collapse(document.documentElement, 0);
  }

  sendAsyncMessage("sdk/test/context-menu/selected");
});
