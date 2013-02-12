/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var PluginCTPHandler = {
  init: function() {
    addEventListener("PluginClickToPlay", this, false);
  },

  addLinkClickCallback: function (linkNode, callbackName /*callbackArgs...*/) {
     // XXX just doing (callback)(arg) was giving a same-origin error. bug?
     let self = this;
     let callbackArgs = Array.prototype.slice.call(arguments).slice(2);
     linkNode.addEventListener("click",
                               function(evt) {
                                 if (!evt.isTrusted)
                                   return;
                                 evt.preventDefault();
                                 if (callbackArgs.length == 0)
                                   callbackArgs = [ evt ];
                                 (self[callbackName]).apply(self, callbackArgs);
                               },
                               true);
 
     linkNode.addEventListener("keydown",
                               function(evt) {
                                 if (!evt.isTrusted)
                                   return;
                                 if (evt.keyCode == evt.DOM_VK_RETURN) {
                                   evt.preventDefault();
                                   if (callbackArgs.length == 0)
                                     callbackArgs = [ evt ];
                                   evt.preventDefault();
                                   (self[callbackName]).apply(self, callbackArgs);
                                 }
                               },
                               true);
   },

  handleEvent : function(event) {
    if (event.type != "PluginClickToPlay")
      return;
    let plugin = event.target;
    PluginHandler.addLinkClickCallback(plugin, "reloadToEnablePlugin");
  },

  reloadToEnablePlugin: function() {
    sendAsyncMessage("Browser:PluginClickToPlayClicked", { });
  }
};

//PluginCTPHandler.init();
