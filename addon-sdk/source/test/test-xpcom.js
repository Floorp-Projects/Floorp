/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const xpcom = require("sdk/platform/xpcom");
const { Cc, Ci, Cm, Cr } = require("chrome");
const { isCIDRegistered } = Cm.QueryInterface(Ci.nsIComponentRegistrar);
const { Class } = require("sdk/core/heritage");
const { Loader } = require("sdk/test/loader");
const { Services } = require("resource://gre/modules/Services.jsm");

exports['test Unknown implements nsISupports'] = function(assert) {
  let actual = xpcom.Unknown();
  assert.equal(actual.QueryInterface(Ci.nsISupports),
               actual,
               'component implements nsISupports');
};

exports['test implement xpcom interfaces'] = function(assert) {
  let WeakReference = Class({
    extends: xpcom.Unknown,
    interfaces: [ 'nsIWeakReference' ],
    QueryReferent: function() {}
  });
  let weakReference = WeakReference()

  assert.equal(weakReference.QueryInterface(Ci.nsISupports),
               weakReference,
               'component implements nsISupports');
  assert.equal(weakReference.QueryInterface(Ci.nsIWeakReference),
               weakReference,
               'component implements specified interface');

  assert.throws(function() {
    component.QueryInterface(Ci.nsIObserver);
  }, "component does not implements interface");

  let Observer = Class({
    extends: WeakReference,
    interfaces: [ 'nsIObserver', 'nsIRequestObserver' ],
    observe: function() {},
    onStartRequest: function() {},
    onStopRequest: function() {}
  });
  let observer = Observer()

  assert.equal(observer.QueryInterface(Ci.nsISupports),
               observer,
               'derived component implements nsISupports');
  assert.equal(observer.QueryInterface(Ci.nsIWeakReference),
               observer,
               'derived component implements supers interface');
  assert.equal(observer.QueryInterface(Ci.nsIObserver),
               observer.QueryInterface(Ci.nsIRequestObserver),
               'derived component implements specified interfaces');
};

exports['test implement factory without contract'] = function(assert) {
  let actual = xpcom.Factory({
    get wrappedJSObject() this,
  });

  assert.ok(isCIDRegistered(actual.id), 'factory is regiseterd');
  xpcom.unregister(actual);
  assert.ok(!isCIDRegistered(actual.id), 'factory is unregistered');
};

exports['test implement xpcom factory'] = function(assert) {
  let Component = Class({
    extends: xpcom.Unknown,
    interfaces: [ 'nsIObserver' ],
    get wrappedJSObject() this,
    observe: function() {}
  });

  let factory = xpcom.Factory({
    register: false,
    contract: '@jetpack/test/factory;1',
    Component: Component
  });

  assert.ok(!isCIDRegistered(factory.id), 'factory is not registered');
  xpcom.register(factory);
  assert.ok(isCIDRegistered(factory.id), 'factory is registered');

  let actual = Cc[factory.contract].createInstance(Ci.nsIObserver);

  assert.ok(actual.wrappedJSObject instanceof Component,
            "createInstance returnes wrapped factory instances");

  assert.notEqual(Cc[factory.contract].createInstance(Ci.nsIObserver),
                  Cc[factory.contract].createInstance(Ci.nsIObserver),
                  "createInstance returns new instance each time");
};

exports['test implement xpcom service'] = function(assert) {
  let actual = xpcom.Service({
    contract: '@jetpack/test/service;1',
    register: false,
    Component: Class({
      extends: xpcom.Unknown,
      interfaces: [ 'nsIObserver'],
      get wrappedJSObject() this,
      observe: function() {},
      name: 'my-service'
    })
  });

  assert.ok(!isCIDRegistered(actual.id), 'component is not registered');
  xpcom.register(actual);
  assert.ok(isCIDRegistered(actual.id), 'service is regiseterd');
  assert.ok(Cc[actual.contract].getService(Ci.nsIObserver).observe,
            'service can be accessed via get service');
  assert.equal(Cc[actual.contract].getService(Ci.nsIObserver).wrappedJSObject,
               actual.component,
               'wrappedJSObject is an actual component');
  xpcom.unregister(actual);
  assert.ok(!isCIDRegistered(actual.id), 'service is unregistered');
};


function testRegister(assert, text) {

  const service = xpcom.Service({
    description: 'test about:boop page',
    contract: '@mozilla.org/network/protocol/about;1?what=boop',
    register: false,
    Component: Class({
      extends: xpcom.Unknown,
      get wrappedJSObject() this,
      interfaces: [ 'nsIAboutModule' ],
      newChannel : function(aURI, aLoadInfo) {
        var ios = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);

        var uri = ios.newURI("data:text/plain;charset=utf-8," + text,
                             null, null);
        var channel = ios.newChannelFromURIWithLoadInfo(uri, aLoadInfo);

        channel.originalURI = aURI;
        return channel;
      },
      getURIFlags: function(aURI) {
        return Ci.nsIAboutModule.ALLOW_SCRIPT;
      }
    })
  });

  xpcom.register(service);

  assert.equal(isCIDRegistered(service.id), true);

  var aboutFactory = xpcom.factoryByContract(service.contract);
  var about = aboutFactory.createInstance(Ci.nsIAboutModule);

  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  assert.equal(
    about.getURIFlags(ios.newURI("http://foo.com", null, null)),
    Ci.nsIAboutModule.ALLOW_SCRIPT
  );

  var aboutURI = ios.newURI("about:boop", null, null);
  var channel = ios.newChannelFromURI2(aboutURI,
                                       null,      // aLoadingNode
                                       Services.scriptSecurityManager.getSystemPrincipal(),
                                       null,      // aTriggeringPrincipal
                                       Ci.nsILoadInfo.SEC_NORMAL,
                                       Ci.nsIContentPolicy.TYPE_OTHER);
  var iStream = channel.open();
  var siStream = Cc['@mozilla.org/scriptableinputstream;1']
                 .createInstance(Ci.nsIScriptableInputStream);
  siStream.init(iStream);
  var data = new String();
  data += siStream.read(-1);
  siStream.close();
  iStream.close();
  assert.equal(data, text);

  xpcom.unregister(service);
  assert.equal(isCIDRegistered(service.id), false);
}

exports["test register"] = function(assert) {
  testRegister(assert, "hai2u");
};

exports["test re-register"] = function(assert) {
  testRegister(assert, "hai2u again");
};

exports["test unload"] = function(assert) {
  let loader = Loader(module);
  let sbxpcom = loader.require("sdk/platform/xpcom");

  let auto = sbxpcom.Factory({
    contract: "@mozilla.org/test/auto-unload;1",
    description: "test auto",
    name: "auto"
  });

  let manual = sbxpcom.Factory({
    contract: "@mozilla.org/test/manual-unload;1",
    description: "test manual",
    register: false,
    unregister: false,
    name: "manual"
  });

  assert.equal(isCIDRegistered(auto.id), true, 'component registered');
  assert.equal(isCIDRegistered(manual.id), false, 'component not registered');

  sbxpcom.register(manual)
  assert.equal(isCIDRegistered(manual.id), true,
                   'component was automatically registered on first instance');
  loader.unload();

  assert.equal(isCIDRegistered(auto.id), false,
                   'component was atumatically unregistered on unload');
  assert.equal(isCIDRegistered(manual.id), true,
                   'component was not automatically unregistered on unload');
  sbxpcom.unregister(manual);
  assert.equal(isCIDRegistered(manual.id), false,
                   'component was manually unregistered on unload');
};

require("sdk/test").run(exports);
