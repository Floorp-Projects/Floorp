/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, manager: Cm} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const PRESENTATION_DEVICE_PROMPT_PATH =
  "chrome://presentation/content/PresentationDevicePrompt.jsm";

function log(aMsg) {
  // dump("@ Presentation: " + aMsg + "\n");
}

function install(aData, aReason) {
}

function uninstall(aData, aReason) {
}

function startup(aData, aReason) {
  log("startup");
  Presentation.init();
}

function shutdown(aData, aReason) {
  log("shutdown");
  Presentation.uninit();
}

// Register/unregister a constructor as a factory.
function Factory() {}
Factory.prototype = {
  register(targetConstructor) {
    let proto = targetConstructor.prototype;
    this._classID = proto.classID;

    let factory = XPCOMUtils._getFactory(targetConstructor);
    this._factory = factory;

    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.registerFactory(proto.classID, proto.classDescription,
                              proto.contractID, factory);
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._classID, this._factory);
    this._factory = null;
    this._classID = null;
  },
};

var Presentation = {
  // PUBLIC APIs
  init() {
    log("init");
    // Register PresentationDevicePrompt into a XPCOM component.
    Cu.import(PRESENTATION_DEVICE_PROMPT_PATH);
    this._register();
  },

  uninit() {
    log("uninit");
    // Unregister PresentationDevicePrompt XPCOM component.
    this._unregister();
    Cu.unload(PRESENTATION_DEVICE_PROMPT_PATH);
  },

  // PRIVATE APIs
  _register() {
    log("_register");
    this._devicePromptFactory = new Factory();
    this._devicePromptFactory.register(PresentationDevicePrompt);
  },

  _unregister() {
    log("_unregister");
    this._devicePromptFactory.unregister();
    delete this._devicePromptFactory;
  },
};
