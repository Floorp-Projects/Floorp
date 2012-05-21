/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function(global) {
   const Cc = Components.classes;
   const Ci = Components.interfaces;
   const Cu = Components.utils;
   const Cr = Components.results;

   var exports = {};

   // Load the SecurableModule prerequisite.
   var securableModule;

   if (global.require)
     // We're being loaded in a SecurableModule.
     securableModule = require("securable-module");
   else {
     var myURI = Components.stack.filename.split(" -> ").slice(-1)[0];
     var ios = Cc['@mozilla.org/network/io-service;1']
               .getService(Ci.nsIIOService);
     var securableModuleURI = ios.newURI("securable-module.js", null,
                                         ios.newURI(myURI, null, null));
     if (securableModuleURI.scheme == "chrome") {
       // The securable-module module is at a chrome URI, so we can't
       // simply load it via Cu.import(). Let's assume we're in a
       // chrome-privileged document and use mozIJSSubScriptLoader.
       var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                    .getService(Ci.mozIJSSubScriptLoader);

       // Import the script, don't pollute the global scope.
       securableModule = {__proto__: global};
       loader.loadSubScript(securableModuleURI.spec, securableModule);
       securableModule = securableModule.SecurableModule;
     } else {
       securableModule = {};
       try {
         Cu.import(securableModuleURI.spec, securableModule);
       } catch (e if e.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
         Cu.reportError("Failed to load " + securableModuleURI.spec);
       }
     }
   }

   function unloadLoader() {
     this.require("unload").send();
   }

   var cuddlefishSandboxFactory = {
     createSandbox: function(options) {
       var filename = options.filename ? options.filename : null;
       var sandbox = this.__proto__.createSandbox(options);
       sandbox.defineProperty("__url__", filename);
       return sandbox;
     },
     __proto__: new securableModule.SandboxFactory("system")
   };

   function CuddlefishModule(loader) {
     this.parentLoader = loader;
     this.__proto__ = exports;
   }

   var Loader = exports.Loader = function Loader(options) {
     var globals = {Cc: Components.classes,
                    Ci: Components.interfaces,
                    Cu: Components.utils,
                    Cr: Components.results};

     if (options.console)
       globals.console = options.console;
     if (options.memory)
       globals.memory = options.memory;

     var modules = {};
     var loaderOptions = {rootPath: options.rootPath,
                          rootPaths: options.rootPaths,
                          fs: options.fs,
                          sandboxFactory: cuddlefishSandboxFactory,
                          globals: globals,
                          modules: modules};

     var loader = new securableModule.Loader(loaderOptions);
     var path = loader.fs.resolveModule(null, "cuddlefish");
     modules[path] = new CuddlefishModule(loader);

     if (!globals.console) {
       var console = loader.require("plain-text-console");
       globals.console = new console.PlainTextConsole(options.print);
     }
     if (!globals.memory)
       globals.memory = loader.require("memory");

     loader.console = globals.console;
     loader.memory = globals.memory;
     loader.unload = unloadLoader;

     return loader;
   };

   if (global.window) {
     // We're being loaded in a chrome window, or a web page with
     // UniversalXPConnect privileges.
     global.Cuddlefish = exports;
   } else if (global.exports) {
     // We're being loaded in a SecurableModule.
     for (name in exports) {
       global.exports[name] = exports[name];
     }
   } else {
     // We're being loaded in a JS module.
     global.EXPORTED_SYMBOLS = [];
     for (name in exports) {
       global.EXPORTED_SYMBOLS.push(name);
       global[name] = exports[name];
     }
   }
 })(this);
