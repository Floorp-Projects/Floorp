// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function PromptService() {

}

PromptService.prototype = {
  classID: Components.ID("{9a61149b-2276-4a0a-b79c-be994ad106cf}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptFactory, Ci.nsIPromptService, Ci.nsIPromptService2]),

  /* ----------  nsIPromptFactory  ---------- */

  getPrompt: function getPrompt(domWin, iid) {
    let factory =
          Components.classesByID["{1c978d25-b37f-43a8-a2d6-0c7a239ead87}"]
                    .createInstance(Ci.nsIPromptFactory);

    let prompt = factory.getPrompt(domWin, iid);
    try {
      let bag = prompt.QueryInterface(Ci.nsIWritablePropertyBag2);
      bag.setPropertyAsBool("allowTabModal", true);
    } catch(e) {
    }
    return prompt;
  },

  /* ----------  private memebers  ---------- */

  callProxy: function(aName, aArgs) {
    let domWin = aArgs[0];
    if (!domWin) {
      let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
      if (!chromeWin) {
        let msg = "PromptService.js: Attempted create a prompt but no DOM Window specified and failed to find one";
        Cu.reportError(msg);
        throw(msg);
      }
      domWin = chromeWin.getBrowser().contentWindow;
    }

    let prompt = this.getPrompt(domWin, Ci.nsIPrompt);
    if (aName == 'promptAuth' || aName == 'promptAuthAsync') {
      let adapterFactory =
            Components.classesByID["{6e134924-6c3a-4d86-81ac-69432dd971dc}"]
                      .createInstance(Ci.nsIAuthPromptAdapterFactory);
      prompt = adapterFactory.createAdapter(prompt);
    }

    return prompt[aName].apply(prompt, Array.prototype.slice.call(aArgs, 1));
  },

  /* ----------  nsIPromptService  ---------- */

  alert: function() {
    return this.callProxy("alert", arguments);
  },
  alertCheck: function() {
    return this.callProxy("alertCheck", arguments);
  },
  confirm: function() {
    return this.callProxy("confirm", arguments);
  },
  confirmCheck: function() {
    return this.callProxy("confirmCheck", arguments);
  },
  confirmEx: function() {
    return this.callProxy("confirmEx", arguments);
  },
  prompt: function() {
    return this.callProxy("prompt", arguments);
  },
  promptUsernameAndPassword: function() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  },
  promptPassword: function() {
    return this.callProxy("promptPassword", arguments);
  },
  select: function() {
    return this.callProxy("select", arguments);
  },

  /* ----------  nsIPromptService2  ---------- */

  promptAuth: function() {
    return this.callProxy("promptAuth", arguments);
  },
  asyncPromptAuth: function() {
    return this.callProxy("asyncPromptAuth", arguments);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PromptService]);
