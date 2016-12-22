/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This code runs in the sandbox in the content process where the page that
 * loaded the plugin lives. It communicates with the process where the PPAPI
 * implementation lives.
 */
const { utils: Cu } = Components;

let mm = pluginElement.frameLoader.messageManager;
let containerWindow = pluginElement.ownerDocument.defaultView;

function mapValue(v, instance) {
  return instance.rt.toPP_Var(v, instance);
}

dump("<>>>>>>>>>>>>>>>>>>>> AHA <<<<<<<<<<<<<<<<<<<<<>\n");
dump(`pluginElement: ${pluginElement.toSource()}\n`);
dump(`pluginElement.frameLoader: ${pluginElement.frameLoader.toSource()}\n`);
dump(`pluginElement.frameLoader.messageManager: ${pluginElement.frameLoader.messageManager.toSource()}\n`);
dump("<>>>>>>>>>>>>>>>>>>>> AHA2 <<<<<<<<<<<<<<<<<<<<<>\n");

mm.addMessageListener("ppapi.js:frameLoaded", ({ target }) => {
  let tagName = pluginElement.nodeName;

  // Getting absolute URL from the EMBED tag
  let url = pluginElement.srcURI.spec;
  let objectParams = new Map();
  for (let i = 0; i < pluginElement.attributes.length; ++i) {
    let paramName = pluginElement.attributes[i].localName;
    objectParams.set(paramName, pluginElement.attributes[i].value);
  }
  if (tagName == "OBJECT") {
    let params = pluginElement.getElementsByTagName("param");
    Array.prototype.forEach.call(params, (p) => {
      var paramName = p.getAttribute("name").toLowerCase();
      objectParams.set(paramName, p.getAttribute("value"));
    });
  }

  let documentURL = pluginElement.ownerDocument.location.href;
  let baseUrl = documentURL;
  if (objectParams.base) {
    try {
      let parsedDocumentUrl = Services.io.newURI(documentURL);
      baseUrl = Services.io.newURI(objectParams.base, null, parsedDocumentUrl).spec;
    } catch (e) { /* ignore */ }
  }

  let info = {
    url,
    documentURL,
    isFullFrame: pluginElement.ownerDocument.mozSyntheticDocument,
    setupJSInstanceObject: true,
    arguments: {
      keys: Array.from(objectParams.keys()),
      values: Array.from(objectParams.values()),
    },
  };

  mm.sendAsyncMessage("ppapi.js:createInstance", { type: "flash", info },
                      { pluginWindow: containerWindow });
});

mm.addMessageListener("ppapiflash.js:executeScript", ({ data, objects: { instance } }) => {
  return mapValue(containerWindow.eval(data), instance);
});

let jsInterfaceObjects = new WeakMap();

mm.addMessageListener("ppapiflash.js:createObject", ({ data: { objectClass, objectData }, objects: { instance, call: callRemote } }) => {
  dump("ppapiflash.js:createObject\n");
  let call = (name, args) => {
/*
    let metaData = target[Symbol.for("metaData")];
    let callObj = { __interface: "PPP_Class_Deprecated;1.0", __instance: metaData.objectClass, __member: name, object: metaData.objectData };
*/
dump("ppapiflash.js:createObject -> call\n");
try {
    let result = callRemote(name, JSON.stringify(args));
} catch (e) {
  dump("FIXME: we rely on CPOWs!\n");
  return undefined;
}
dump("RESULT: " + result[0] + "\n");
dump("EXCEPTION: " + result[1] + "\n");
    return result[0];
  };

  const handler = {
    // getPrototypeOf -> target (see bug 888969)
    // setPrototypeOf -> target (see bug 888969)
    // isExtensible -> target
    // preventExtensions -> target
    getOwnPropertyDescriptor: function(target, name) {
      let value = this.get(target, name);
      if (this._hasProperty(target, name)) {
        return {
          "writable": true,
          "enumerable": true,
          "configurable": true,
          "get": this.get.bind(this, target, name),
          "set": this.set.bind(this, target, name),
        };
      }

      if (this._hasMethod(target, name)) {
        return {
          "value": (...args) => {
            //call("Call", { name, argc, argv, exception });
          },
          "writable": true,
          "enumerable": true,
          "configurable": true,
        };
      }

      return undefined;
    },
    // defineProperty -> target
    has: function(target, name) {
      return this._hasProperty(target, name) || this._hasMethod(target, name);
    },
    get: function(target, name, receiver) {
      dump(`Calling GetProperty for ${name.toSource()}\n`);
      let prop = call("GetProperty", { name });
      if (!prop || PP_VarType[prop.type] == PP_VarType.PP_VARTYPE_UNDEFINED) {
        // FIXME Need to get the exception!
        return undefined;
      }
      return prop;
    },
    set: function(target, name, value, receiver) {
      call("SetProperty", { name, value });
      // FIXME Need to get the exception!
      return true;
    },
    deleteProperty: function(target, name) {
      call("RemoveProperty", { name });
      // FIXME Need to get the exception!
      return true;
    },
    enumerate: function(target) {
      let keys = this.ownKeys(target);
      return keys[Symbol.iterator];
    },
    ownKeys: function(target) {
      let result = call("GetAllPropertyNames");
      dump(result.toSource() + "\n");
      dump(typeof result.properties + "\n");
      return result.properties;
    },
    apply: function(target, thisArg, args) {
    },
    construct: function(target, args) {
      //call("Construct", { argc, argv, exception });
    },

    _hasProperty: function(target, name) {
      return call("HasProperty", { name }) == 1 /* PP_Bool.PP_TRUE */;
    },
    _hasMethod: function(target, name) {
      return call("HasMethod", { name }) == 1 /* PP_Bool.PP_TRUE */;
    },
  };

  let metaData = {
    objectClass,
    objectData,
  };

  let clonedHandler = Cu.cloneInto(handler, containerWindow, { cloneFunctions: true });
  let clonedMetaData = Cu.cloneInto(metaData, containerWindow, { cloneFunctions: true });
  let target = Cu.createObjectIn(containerWindow);
  target[Symbol.for("metaData")] = clonedMetaData;

dump("IN CREATEOBJECT\n");
  let proxy = new containerWindow.Proxy(target, clonedHandler);
  jsInterfaceObjects.set(proxy, metaData);
  let foo = mapValue(proxy.wrappedJSObject, instance);
dump("CREATEDOBJECT: " + foo.toSource() + "\n");
  return foo;
});

mm.addMessageListener("ppapiflash.js:isInstanceOf", ({ data: { objectClass }, objects: { object, instance } }) => {
  let metaData = jsInterfaceObjects.get(object);
  if (!metaData || metaData.objectClass != objectClass) {
    return [false];
  }
  return [true, metaData.objectData];
});

mm.addMessageListener("ppapiflash.js:getProperty", ({ data: { name }, objects: { object, instance } }) => {
  try {
    return [mapValue(object[name], instance), { exception: null }];
  } catch (e) {
    return [mapValue(null, instance), { exception: mapValue(e, instance) }];
  }
});

mm.addMessageListener("ppapiflash.js:log", ({ data }) => {
  containerWindow.console.log(data);
});

mm.addMessageListener("ppapiflash.js:setInstancePrototype", ({ objects: { proto } }) => {
  Object.setPrototypeOf(proto.wrappedJSObject, Object.getPrototypeOf(pluginElement.wrappedJSObject));
  Object.setPrototypeOf(pluginElement.wrappedJSObject, proto.wrappedJSObject);
});

mm.addMessageListener("ppapi.js:isFullscreen", () => {
  return containerWindow.document.fullscreenElement == pluginElement;
});

mm.addMessageListener("ppapi.js:setFullscreen", ({ data }) => {
  if (data) {
    pluginElement.requestFullscreen();
  } else {
    containerWindow.document.exitFullscreen();
  }
});

mm.loadFrameScript("resource://ppapi.js/ppapi-instance.js", true);
