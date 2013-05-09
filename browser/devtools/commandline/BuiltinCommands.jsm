/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const BRAND_SHORT_NAME = Cc["@mozilla.org/intl/stringbundle;1"]
                           .getService(Ci.nsIStringBundleService)
                           .createBundle("chrome://branding/locale/brand.properties")
                           .GetStringFromName("brandShortName");

this.EXPORTED_SYMBOLS = [ "CmdAddonFlags", "CmdCommands" ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource:///modules/devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource:///modules/devtools/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppCacheUtils",
                                  "resource:///modules/devtools/AppCacheUtils.jsm");

/* CmdAddon ---------------------------------------------------------------- */

(function(module) {
  XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                    "resource://gre/modules/AddonManager.jsm");

  // We need to use an object in which to store any flags because a primitive
  // would remain undefined.
  module.CmdAddonFlags = {
    addonsLoaded: false
  };

  /**
  * 'addon' command.
  */
  gcli.addCommand({
    name: "addon",
    description: gcli.lookup("addonDesc")
  });

  /**
  * 'addon list' command.
  */
  gcli.addCommand({
    name: "addon list",
    description: gcli.lookup("addonListDesc"),
    returnType: "addonsInfo",
    params: [{
      name: 'type',
      type: {
        name: 'selection',
        data: ["dictionary", "extension", "locale", "plugin", "theme", "all"]
      },
      defaultValue: 'all',
      description: gcli.lookup("addonListTypeDesc")
    }],
    exec: function(aArgs, context) {
      let deferred = context.defer();
      function pendingOperations(aAddon) {
        let allOperations = ["PENDING_ENABLE",
                             "PENDING_DISABLE",
                             "PENDING_UNINSTALL",
                             "PENDING_INSTALL",
                             "PENDING_UPGRADE"];
        return allOperations.reduce(function(operations, opName) {
          return aAddon.pendingOperations & AddonManager[opName] ?
            operations.concat(opName) :
            operations;
        }, []);
      }
      let types = aArgs.type === "all" ? null : [aArgs.type];
      AddonManager.getAddonsByTypes(types, function(addons) {
        deferred.resolve({
          addons: addons.map(function(addon) {
            return {
              name: addon.name,
              version: addon.version,
              isActive: addon.isActive,
              pendingOperations: pendingOperations(addon)
            };
          }),
          type: aArgs.type
        });
      });
      return deferred.promise;
    }
  });

  gcli.addConverter({
    from: "addonsInfo",
    to: "view",
    exec: function(addonsInfo, context) {
      if (!addonsInfo.addons.length) {
        return context.createView({
          html: "<p>${message}</p>",
          data: { message: gcli.lookup("addonNoneOfType") }
        });
      }

      let headerLookups = {
        "dictionary": "addonListDictionaryHeading",
        "extension": "addonListExtensionHeading",
        "locale": "addonListLocaleHeading",
        "plugin": "addonListPluginHeading",
        "theme": "addonListThemeHeading",
        "all": "addonListAllHeading"
      };
      let header = gcli.lookup(headerLookups[addonsInfo.type] ||
                               "addonListUnknownHeading");

      let operationLookups = {
        "PENDING_ENABLE": "addonPendingEnable",
        "PENDING_DISABLE": "addonPendingDisable",
        "PENDING_UNINSTALL": "addonPendingUninstall",
        "PENDING_INSTALL": "addonPendingInstall",
        "PENDING_UPGRADE": "addonPendingUpgrade"
      };
      function lookupOperation(opName) {
        let lookupName = operationLookups[opName];
        return lookupName ? gcli.lookup(lookupName) : opName;
      }

      function arrangeAddons(addons) {
        let enabledAddons = [];
        let disabledAddons = [];
        addons.forEach(function(aAddon) {
          if (aAddon.isActive) {
            enabledAddons.push(aAddon);
          } else {
            disabledAddons.push(aAddon);
          }
        });

        function compareAddonNames(aNameA, aNameB) {
          return String.localeCompare(aNameA.name, aNameB.name);
        }
        enabledAddons.sort(compareAddonNames);
        disabledAddons.sort(compareAddonNames);

        return enabledAddons.concat(disabledAddons);
      }

      function isActiveForToggle(addon) {
        return (addon.isActive && ~~addon.pendingOperations.indexOf("PENDING_DISABLE"));
      }

      return context.createView({
        html: addonsListHtml,
        data: {
          header: header,
          addons: arrangeAddons(addonsInfo.addons).map(function(addon) {
            return {
              name: addon.name,
              label: addon.name.replace(/\s/g, "_") +
                    (addon.version ? "_" + addon.version : ""),
              status: addon.isActive ? "enabled" : "disabled",
              version: addon.version,
              pendingOperations: addon.pendingOperations.length ?
                (" (" + gcli.lookup("addonPending") + ": "
                 + addon.pendingOperations.map(lookupOperation).join(", ")
                 + ")") :
                "",
              toggleActionName: isActiveForToggle(addon) ? "disable": "enable",
              toggleActionMessage: isActiveForToggle(addon) ?
                gcli.lookup("addonListOutDisable") :
                gcli.lookup("addonListOutEnable")
            };
          }),
          onclick: createUpdateHandler(context),
          ondblclick: createExecuteHandler(context)
        }
      });
    }
  });

  var addonsListHtml = "" +
        "<table>" +
        " <caption>${header}</caption>" +
        " <tbody>" +
        "  <tr foreach='addon in ${addons}'" +
        "      class=\"gcli-addon-${addon.status}\">" +
        "    <td>${addon.name} ${addon.version}</td>" +
        "    <td>${addon.pendingOperations}</td>" +
        "    <td>" +
        "      <span class='gcli-out-shortcut'" +
        "            data-command='addon ${addon.toggleActionName} ${addon.label}'" +
        "       onclick='${onclick}'" +
        "       ondblclick='${ondblclick}'" +
        "      >${addon.toggleActionMessage}</span>" +
        "    </td>" +
        "  </tr>" +
        " </tbody>" +
        "</table>" +
        "";

  // We need a list of addon names for the enable and disable commands. Because
  // getting the name list is async we do not add the commands until we have the
  // list.
  AddonManager.getAllAddons(function addonAsync(aAddons) {
    // We listen for installs to keep our addon list up to date. There is no need
    // to listen for uninstalls because uninstalled addons are simply disabled
    // until restart (to enable undo functionality).
    AddonManager.addAddonListener({
      onInstalled: function(aAddon) {
        addonNameCache.push({
          name: representAddon(aAddon).replace(/\s/g, "_"),
          value: aAddon.name
        });
      },
      onUninstalled: function(aAddon) {
        let name = representAddon(aAddon).replace(/\s/g, "_");

        for (let i = 0; i < addonNameCache.length; i++) {
          if(addonNameCache[i].name == name) {
            addonNameCache.splice(i, 1);
            break;
          }
        }
      },
    });

    /**
    * Returns a string that represents the passed add-on.
    */
    function representAddon(aAddon) {
      let name = aAddon.name + " " + aAddon.version;
      return name.trim();
    }

    let addonNameCache = [];

    // The name parameter, used in "addon enable" and "addon disable."
    let nameParameter = {
      name: "name",
      type: {
        name: "selection",
        lookup: addonNameCache
      },
      description: gcli.lookup("addonNameDesc")
    };

    for (let addon of aAddons) {
      addonNameCache.push({
        name: representAddon(addon).replace(/\s/g, "_"),
        value: addon.name
      });
    }

    /**
    * 'addon enable' command.
    */
    gcli.addCommand({
      name: "addon enable",
      description: gcli.lookup("addonEnableDesc"),
      params: [nameParameter],
      exec: function(aArgs, context) {
        /**
        * Enables the addon in the passed list which has a name that matches
        * according to the passed name comparer, and resolves the promise which
        * is the scope (this) of this function to display the result of this
        * enable attempt.
        */
        function enable(aName, addons) {
          // Find the add-on.
          let addon = null;
          addons.some(function(candidate) {
            if (candidate.name == aName) {
              addon = candidate;
              return true;
            } else {
              return false;
            }
          });

          let name = representAddon(addon);
          let message = "";

          if (!addon.userDisabled) {
            message = gcli.lookupFormat("addonAlreadyEnabled", [name]);
          } else {
            addon.userDisabled = false;
            message = gcli.lookupFormat("addonEnabled", [name]);
          }
          this.resolve(message);
        }

        let deferred = context.defer();
        // List the installed add-ons, enable one when done listing.
        AddonManager.getAllAddons(enable.bind(deferred, aArgs.name));
        return deferred.promise;
      }
    });

    /**
    * 'addon disable' command.
    */
    gcli.addCommand({
      name: "addon disable",
      description: gcli.lookup("addonDisableDesc"),
      params: [nameParameter],
      exec: function(aArgs, context) {
        /**
        * Like enable, but ... you know ... the exact opposite.
        */
        function disable(aName, addons) {
          // Find the add-on.
          let addon = null;
          addons.some(function(candidate) {
            if (candidate.name == aName) {
              addon = candidate;
              return true;
            } else {
              return false;
            }
          });

          let name = representAddon(addon);
          let message = "";

          if (addon.userDisabled) {
            message = gcli.lookupFormat("addonAlreadyDisabled", [name]);
          } else {
            addon.userDisabled = true;
            message = gcli.lookupFormat("addonDisabled", [name]);
          }
          this.resolve(message);
        }

        let deferred = context.defer();
        // List the installed add-ons, disable one when done listing.
        AddonManager.getAllAddons(disable.bind(deferred, aArgs.name));
        return deferred.promise;
      }
    });
    module.CmdAddonFlags.addonsLoaded = true;
    Services.obs.notifyObservers(null, "gcli_addon_commands_ready", null);
  });

  /**
   * Helper to find the 'data-command' attribute and call some action on it.
   * @see |updateCommand()| and |executeCommand()|
   */
  function withCommand(element, action) {
    var command = element.getAttribute("data-command");
    if (!command) {
      command = element.querySelector("*[data-command]")
        .getAttribute("data-command");
    }

    if (command) {
      action(command);
    }
    else {
      console.warn("Missing data-command for " + util.findCssSelector(element));
    }
  }

  /**
   * Create a handler to update the requisition to contain the text held in the
   * first matching data-command attribute under the currentTarget of the event.
   * @param context Either a Requisition or an ExecutionContext or another object
   * that contains an |update()| function that follows a similar contract.
   */
  function createUpdateHandler(context) {
    return function(ev) {
      withCommand(ev.currentTarget, function(command) {
        context.update(command);
      });
    }
  }

  /**
   * Create a handler to execute the text held in the data-command attribute
   * under the currentTarget of the event.
   * @param context Either a Requisition or an ExecutionContext or another object
   * that contains an |update()| function that follows a similar contract.
   */
  function createExecuteHandler(context) {
    return function(ev) {
      withCommand(ev.currentTarget, function(command) {
        context.exec({
          visible: true,
          typed: command
        });
      });
    }
  }

}(this));

/* CmdCalllog -------------------------------------------------------------- */

(function(module) {
  XPCOMUtils.defineLazyGetter(this, "Debugger", function() {
    let JsDebugger = {};
    Components.utils.import("resource://gre/modules/jsdebugger.jsm", JsDebugger);

    let global = Components.utils.getGlobalForObject({});
    JsDebugger.addDebuggerToGlobal(global);

    return global.Debugger;
  });

  let debuggers = [];

  /**
  * 'calllog' command
  */
  gcli.addCommand({
    name: "calllog",
    description: gcli.lookup("calllogDesc")
  })

  /**
  * 'calllog start' command
  */
  gcli.addCommand({
    name: "calllog start",
    description: gcli.lookup("calllogStartDesc"),

    exec: function(args, context) {
      let contentWindow = context.environment.contentDocument.defaultView;

      let dbg = new Debugger(contentWindow);
      dbg.onEnterFrame = function(frame) {
        // BUG 773652 -  Make the output from the GCLI calllog command nicer
        contentWindow.console.log("Method call: " + this.callDescription(frame));
      }.bind(this);

      debuggers.push(dbg);

      let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
      let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(target, "webconsole");

      return gcli.lookup("calllogStartReply");
    },

    callDescription: function(frame) {
      let name = "<anonymous>";
      if (frame.callee.name) {
        name = frame.callee.name;
      }
      else {
        let desc = frame.callee.getOwnPropertyDescriptor("displayName");
        if (desc && desc.value && typeof desc.value == "string") {
          name = desc.value;
        }
      }

      let args = frame.arguments.map(this.valueToString).join(", ");
      return name + "(" + args + ")";
    },

    valueToString: function(value) {
      if (typeof value !== "object" || value === null) {
        return uneval(value);
      }
      return "[object " + value.class + "]";
    }
  });

  /**
  * 'calllog stop' command
  */
  gcli.addCommand({
    name: "calllog stop",
    description: gcli.lookup("calllogStopDesc"),

    exec: function(args, context) {
      let numDebuggers = debuggers.length;
      if (numDebuggers == 0) {
        return gcli.lookup("calllogStopNoLogging");
      }

      for (let dbg of debuggers) {
        dbg.onEnterFrame = undefined;
      }
      debuggers = [];

      return gcli.lookupFormat("calllogStopReply", [ numDebuggers ]);
    }
  });
}(this));

/* CmdCalllogChrome -------------------------------------------------------- */

(function(module) {
  XPCOMUtils.defineLazyGetter(this, "Debugger", function() {
    let JsDebugger = {};
    Cu.import("resource://gre/modules/jsdebugger.jsm", JsDebugger);

    let global = Components.utils.getGlobalForObject({});
    JsDebugger.addDebuggerToGlobal(global);

    return global.Debugger;
  });

  let debuggers = [];
  let sandboxes = [];

  /**
  * 'calllog chromestart' command
  */
  gcli.addCommand({
    name: "calllog chromestart",
    description: gcli.lookup("calllogChromeStartDesc"),
    get hidden() gcli.hiddenByChromePref(),
    params: [
      {
        name: "sourceType",
        type: {
          name: "selection",
          data: ["content-variable", "chrome-variable", "jsm", "javascript"]
        }
      },
      {
        name: "source",
        type: "string",
        description: gcli.lookup("calllogChromeSourceTypeDesc"),
        manual: gcli.lookup("calllogChromeSourceTypeManual"),
      }
    ],
    exec: function(args, context) {
      let globalObj;
      let contentWindow = context.environment.contentDocument.defaultView;

      if (args.sourceType == "jsm") {
        try {
          globalObj = Cu.import(args.source);
        }
        catch (e) {
          return gcli.lookup("callLogChromeInvalidJSM");
        }
      } else if (args.sourceType == "content-variable") {
        if (args.source in contentWindow) {
          globalObj = Cu.getGlobalForObject(contentWindow[args.source]);
        } else {
          throw new Error(gcli.lookup("callLogChromeVarNotFoundContent"));
        }
      } else if (args.sourceType == "chrome-variable") {
        let chromeWin = context.environment.chromeDocument.defaultView;
        if (args.source in chromeWin) {
          globalObj = Cu.getGlobalForObject(chromeWin[args.source]);
        } else {
          return gcli.lookup("callLogChromeVarNotFoundChrome");
        }
      } else {
        let chromeWin = context.environment.chromeDocument.defaultView;
        let sandbox = new Cu.Sandbox(chromeWin,
                                    {
                                      sandboxPrototype: chromeWin,
                                      wantXrays: false,
                                      sandboxName: "gcli-cmd-calllog-chrome"
                                    });
        let returnVal;
        try {
          returnVal = Cu.evalInSandbox(args.source, sandbox, "ECMAv5");
          sandboxes.push(sandbox);
        } catch(e) {
          // We need to save the message before cleaning up else e contains a dead
          // object.
          let msg = gcli.lookup("callLogChromeEvalException") + ": " + e;
          Cu.nukeSandbox(sandbox);
          return msg;
        }

        if (typeof returnVal == "undefined") {
          return gcli.lookup("callLogChromeEvalNeedsObject");
        }

        globalObj = Cu.getGlobalForObject(returnVal);
      }

      let dbg = new Debugger(globalObj);
      debuggers.push(dbg);

      dbg.onEnterFrame = function(frame) {
        // BUG 773652 -  Make the output from the GCLI calllog command nicer
        contentWindow.console.log(gcli.lookup("callLogChromeMethodCall") +
                                  ": " + this.callDescription(frame));
      }.bind(this);

      let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
      let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(target, "webconsole");

      return gcli.lookup("calllogChromeStartReply");
    },

    valueToString: function(value) {
      if (typeof value !== "object" || value === null)
        return uneval(value);
      return "[object " + value.class + "]";
    },

    callDescription: function(frame) {
      let name = frame.callee.name || gcli.lookup("callLogChromeAnonFunction");
      let args = frame.arguments.map(this.valueToString).join(", ");
      return name + "(" + args + ")";
    }
  });

  /**
  * 'calllog chromestop' command
  */
  gcli.addCommand({
    name: "calllog chromestop",
    description: gcli.lookup("calllogChromeStopDesc"),
    get hidden() gcli.hiddenByChromePref(),
    exec: function(args, context) {
      let numDebuggers = debuggers.length;
      if (numDebuggers == 0) {
        return gcli.lookup("calllogChromeStopNoLogging");
      }

      for (let dbg of debuggers) {
        dbg.onEnterFrame = undefined;
        dbg.enabled = false;
      }
      for (let sandbox of sandboxes) {
        Cu.nukeSandbox(sandbox);
      }
      debuggers = [];
      sandboxes = [];

      return gcli.lookupFormat("calllogChromeStopReply", [ numDebuggers ]);
    }
  });
}(this));

/* CmdCmd ------------------------------------------------------------------ */

(function(module) {
  let prefSvc = "@mozilla.org/preferences-service;1";
  XPCOMUtils.defineLazyGetter(this, "prefBranch", function() {
    let prefService = Cc[prefSvc].getService(Ci.nsIPrefService);
    return prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);
  });

  XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                    "resource://gre/modules/NetUtil.jsm");
  XPCOMUtils.defineLazyModuleGetter(this, "console",
                                    "resource://gre/modules/devtools/Console.jsm");

  const PREF_DIR = "devtools.commands.dir";

  /**
  * A place to store the names of the commands that we have added as a result of
  * calling refreshAutoCommands(). Used by refreshAutoCommands to remove the
  * added commands.
  */
  let commands = [];

  /**
  * Exported API
  */
  this.CmdCommands = {
    /**
    * Called to look in a directory pointed at by the devtools.commands.dir pref
    * for *.mozcmd files which are then loaded.
    * @param nsIPrincipal aSandboxPrincipal Scope object for the Sandbox in which
    * we eval the script from the .mozcmd file. This should be a chrome window.
    */
    refreshAutoCommands: function GC_refreshAutoCommands(aSandboxPrincipal) {
      // First get rid of the last set of commands
      commands.forEach(function(name) {
        gcli.removeCommand(name);
      });

      let dirName = prefBranch.getComplexValue(PREF_DIR,
                                              Ci.nsISupportsString).data.trim();
      if (dirName == "") {
        return;
      }

      // replaces ~ with the home directory path in unix and windows
      if (dirName.indexOf("~") == 0) {
        let dirService = Cc["@mozilla.org/file/directory_service;1"]
                          .getService(Ci.nsIProperties);
        let homeDirFile = dirService.get("Home", Ci.nsIFile);
        let homeDir = homeDirFile.path;
        dirName = dirName.substr(1);
        dirName = homeDir + dirName;
      }

      let promise = OS.File.stat(dirName);
      promise = promise.then(
        function onSuccess(stat) {
          if (!stat.isDir) {
            throw new Error('\'' + dirName + '\' is not a directory.');
          } else {
            return dirName;
          }
        },
        function onFailure(reason) {
          if (reason instanceof OS.File.Error && reason.becauseNoSuchFile) {
            throw new Error('\'' + dirName + '\' does not exist.');
          } else {
            throw reason;
          }
        }
      );

      promise.then(
        function onSuccess() {
          let iterator = new OS.File.DirectoryIterator(dirName);
          let iterPromise = iterator.forEach(
            function onEntry(entry) {
              if (entry.name.match(/.*\.mozcmd$/) && !entry.isDir) {
                loadCommandFile(entry, aSandboxPrincipal);
              }
            }
          );

          iterPromise.then(
            function onSuccess() {
              iterator.close();
            },
            function onFailure(reason) {
              iterator.close();
              throw reason;
            }
          );
        }
      );
    }
  };

  /**
  * Load the commands from a single file
  * @param OS.File.DirectoryIterator.Entry aFileEntry The DirectoryIterator
  * Entry of the file containing the commands that we should read
  * @param nsIPrincipal aSandboxPrincipal Scope object for the Sandbox in which
  * we eval the script from the .mozcmd file. This should be a chrome window.
  */
  function loadCommandFile(aFileEntry, aSandboxPrincipal) {
    let promise = OS.File.read(aFileEntry.path);
    promise = promise.then(
      function onSuccess(array) {
        let decoder = new TextDecoder();
        let source = decoder.decode(array);

        let sandbox = new Cu.Sandbox(aSandboxPrincipal, {
          sandboxPrototype: aSandboxPrincipal,
          wantXrays: false,
          sandboxName: aFileEntry.path
        });
        let data = Cu.evalInSandbox(source, sandbox, "1.8", aFileEntry.name, 1);

        if (!Array.isArray(data)) {
          console.error("Command file '" + aFileEntry.name + "' does not have top level array.");
          return;
        }

        data.forEach(function(commandSpec) {
          gcli.addCommand(commandSpec);
          commands.push(commandSpec.name);
        });

      },
      function onError(reason) {
        console.error("OS.File.read(" + aFileEntry.path + ") failed.");
        throw reason;
      }
    );
  }

  /**
  * 'cmd' command
  */
  gcli.addCommand({
    name: "cmd",
    get hidden() { return !prefBranch.prefHasUserValue(PREF_DIR); },
    description: gcli.lookup("cmdDesc")
  });

  /**
  * 'cmd refresh' command
  */
  gcli.addCommand({
    name: "cmd refresh",
    description: gcli.lookup("cmdRefreshDesc"),
    get hidden() { return !prefBranch.prefHasUserValue(PREF_DIR); },
    exec: function Command_cmdRefresh(args, context) {
      let chromeWindow = context.environment.chromeDocument.defaultView;
      CmdCommands.refreshAutoCommands(chromeWindow);
    }
  });
}(this));

/* CmdConsole -------------------------------------------------------------- */

(function(module) {
  XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
                                    "resource:///modules/HUDService.jsm");

  /**
  * 'console' command
  */
  gcli.addCommand({
    name: "console",
    description: gcli.lookup("consoleDesc"),
    manual: gcli.lookup("consoleManual")
  });

  /**
  * 'console clear' command
  */
  gcli.addCommand({
    name: "console clear",
    description: gcli.lookup("consoleclearDesc"),
    exec: function Command_consoleClear(args, context) {
      let window = context.environment.contentDocument.defaultView;
      let hud = HUDService.getHudByWindow(window);
      // hud will be null if the web console has not been opened for this window
      if (hud) {
        hud.jsterm.clearOutput();
      }
    }
  });

  /**
  * 'console close' command
  */
  gcli.addCommand({
    name: "console close",
    description: gcli.lookup("consolecloseDesc"),
    exec: function Command_consoleClose(args, context) {
      let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
      let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
      return gDevTools.closeToolbox(target);
    }
  });

  /**
  * 'console open' command
  */
  gcli.addCommand({
    name: "console open",
    description: gcli.lookup("consoleopenDesc"),
    exec: function Command_consoleOpen(args, context) {
      let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
      let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
      return gDevTools.showToolbox(target, "webconsole");
    }
  });
}(this));

/* CmdCookie --------------------------------------------------------------- */

(function(module) {
  XPCOMUtils.defineLazyModuleGetter(this, "console",
                                    "resource://gre/modules/devtools/Console.jsm");

  const cookieMgr = Cc["@mozilla.org/cookiemanager;1"]
                      .getService(Ci.nsICookieManager2);

  /**
   * The template for the 'cookie list' command.
   */
  let cookieListHtml = "" +
    "<ul class='gcli-cookielist-list'>" +
    "  <li foreach='cookie in ${cookies}'>" +
    "    <div>${cookie.name}=${cookie.value}</div>" +
    "    <table class='gcli-cookielist-detail'>" +
    "      <tr>" +
    "        <td>" + gcli.lookup("cookieListOutHost") + "</td>" +
    "        <td>${cookie.host}</td>" +
    "      </tr>" +
    "      <tr>" +
    "        <td>" + gcli.lookup("cookieListOutPath") + "</td>" +
    "        <td>${cookie.path}</td>" +
    "      </tr>" +
    "      <tr>" +
    "        <td>" + gcli.lookup("cookieListOutExpires") + "</td>" +
    "        <td>${cookie.expires}</td>" +
    "      </tr>" +
    "      <tr>" +
    "        <td>" + gcli.lookup("cookieListOutAttributes") + "</td>" +
    "        <td>${cookie.attrs}</td>" +
    "      </tr>" +
    "      <tr><td colspan='2'>" +
    "        <span class='gcli-out-shortcut' onclick='${onclick}'" +
    "            data-command='cookie set ${cookie.name} '" +
    "            >" + gcli.lookup("cookieListOutEdit") + "</span>" +
    "        <span class='gcli-out-shortcut'" +
    "            onclick='${onclick}' ondblclick='${ondblclick}'" +
    "            data-command='cookie remove ${cookie.name}'" +
    "            >" + gcli.lookup("cookieListOutRemove") + "</span>" +
    "      </td></tr>" +
    "    </table>" +
    "  </li>" +
    "</ul>" +
    "";

  gcli.addConverter({
    from: "cookies",
    to: "view",
    exec: function(cookies, context) {
      if (cookies.length == 0) {
        let host = context.environment.document.location.host;
        let msg = gcli.lookupFormat("cookieListOutNoneHost", [ host ]);
        return context.createView({ html: "<span>" + msg + "</span>" });
      }

      for (let cookie of cookies) {
        cookie.expires = translateExpires(cookie.expires);

        let noAttrs = !cookie.secure && !cookie.httpOnly && !cookie.sameDomain;
        cookie.attrs = (cookie.secure ? 'secure' : ' ') +
                       (cookie.httpOnly ? 'httpOnly' : ' ') +
                       (cookie.sameDomain ? 'sameDomain' : ' ') +
                       (noAttrs ? gcli.lookup("cookieListOutNone") : ' ');
      }

      return context.createView({
        html: cookieListHtml,
        data: {
          options: { allowEval: true },
          cookies: cookies,
          onclick: createUpdateHandler(context),
          ondblclick: createExecuteHandler(context),
        }
      });
    }
  });

  /**
   * The cookie 'expires' value needs converting into something more readable
   */
  function translateExpires(expires) {
    if (expires == 0) {
      return gcli.lookup("cookieListOutSession");
    }
    return new Date(expires).toLocaleString();
  }

  /**
   * Check if a given cookie matches a given host
   */
  function isCookieAtHost(cookie, host) {
    if (cookie.host == null) {
      return host == null;
    }
    if (cookie.host.startsWith(".")) {
      return cookie.host === "." + host;
    }
    else {
      return cookie.host == host;
    }
  }

  /**
   * 'cookie' command
   */
  gcli.addCommand({
    name: "cookie",
    description: gcli.lookup("cookieDesc"),
    manual: gcli.lookup("cookieManual")
  });

  /**
   * 'cookie list' command
   */
  gcli.addCommand({
    name: "cookie list",
    description: gcli.lookup("cookieListDesc"),
    manual: gcli.lookup("cookieListManual"),
    returnType: "cookies",
    exec: function Command_cookieList(args, context) {
      let host = context.environment.document.location.host;
      if (host == null) {
        throw new Error(gcli.lookup("cookieListOutNonePage"));
      }

      let enm = cookieMgr.getCookiesFromHost(host);

      let cookies = [];
      while (enm.hasMoreElements()) {
        let cookie = enm.getNext().QueryInterface(Ci.nsICookie);
        if (isCookieAtHost(cookie, host)) {
          cookies.push({
            host: cookie.host,
            name: cookie.name,
            value: cookie.value,
            path: cookie.path,
            expires: cookie.expires,
            secure: cookie.secure,
            httpOnly: cookie.httpOnly,
            sameDomain: cookie.sameDomain
          });
        }
      }

      return cookies;
    }
  });

  /**
   * 'cookie remove' command
   */
  gcli.addCommand({
    name: "cookie remove",
    description: gcli.lookup("cookieRemoveDesc"),
    manual: gcli.lookup("cookieRemoveManual"),
    params: [
      {
        name: "name",
        type: "string",
        description: gcli.lookup("cookieRemoveKeyDesc"),
      }
    ],
    exec: function Command_cookieRemove(args, context) {
      let host = context.environment.document.location.host;
      let enm = cookieMgr.getCookiesFromHost(host);

      let cookies = [];
      while (enm.hasMoreElements()) {
        let cookie = enm.getNext().QueryInterface(Components.interfaces.nsICookie);
        if (isCookieAtHost(cookie, host)) {
          if (cookie.name == args.name) {
            cookieMgr.remove(cookie.host, cookie.name, cookie.path, false);
          }
        }
      }
    }
  });

  /**
   * 'cookie set' command
   */
  gcli.addCommand({
    name: "cookie set",
    description: gcli.lookup("cookieSetDesc"),
    manual: gcli.lookup("cookieSetManual"),
    params: [
      {
        name: "name",
        type: "string",
        description: gcli.lookup("cookieSetKeyDesc")
      },
      {
        name: "value",
        type: "string",
        description: gcli.lookup("cookieSetValueDesc")
      },
      {
        group: gcli.lookup("cookieSetOptionsDesc"),
        params: [
          {
            name: "path",
            type: "string",
            defaultValue: "/",
            description: gcli.lookup("cookieSetPathDesc")
          },
          {
            name: "domain",
            type: "string",
            defaultValue: null,
            description: gcli.lookup("cookieSetDomainDesc")
          },
          {
            name: "secure",
            type: "boolean",
            description: gcli.lookup("cookieSetSecureDesc")
          },
          {
            name: "httpOnly",
            type: "boolean",
            description: gcli.lookup("cookieSetHttpOnlyDesc")
          },
          {
            name: "session",
            type: "boolean",
            description: gcli.lookup("cookieSetSessionDesc")
          },
          {
            name: "expires",
            type: "string",
            defaultValue: "Jan 17, 2038",
            description: gcli.lookup("cookieSetExpiresDesc")
          },
        ]
      }
    ],
    exec: function Command_cookieSet(args, context) {
      let host = context.environment.document.location.host;
      let time = Date.parse(args.expires) / 1000;

      cookieMgr.add(args.domain ? "." + args.domain : host,
                    args.path ? args.path : "/",
                    args.name,
                    args.value,
                    args.secure,
                    args.httpOnly,
                    args.session,
                    time);
    }
  });

  /**
   * Helper to find the 'data-command' attribute and call some action on it.
   * @see |updateCommand()| and |executeCommand()|
   */
  function withCommand(element, action) {
    let command = element.getAttribute("data-command");
    if (!command) {
      command = element.querySelector("*[data-command]")
              .getAttribute("data-command");
    }

    if (command) {
      action(command);
    }
    else {
      console.warn("Missing data-command for " + util.findCssSelector(element));
    }
  }

  /**
   * Create a handler to update the requisition to contain the text held in the
   * first matching data-command attribute under the currentTarget of the event.
   * @param context Either a Requisition or an ExecutionContext or another object
   * that contains an |update()| function that follows a similar contract.
   */
  function createUpdateHandler(context) {
    return function(ev) {
      withCommand(ev.currentTarget, function(command) {
        context.update(command);
      });
    }
  }

  /**
   * Create a handler to execute the text held in the data-command attribute
   * under the currentTarget of the event.
   * @param context Either a Requisition or an ExecutionContext or another object
   * that contains an |update()| function that follows a similar contract.
   */
  function createExecuteHandler(context) {
    return function(ev) {
      withCommand(ev.currentTarget, function(command) {
        context.exec({
          visible: true,
          typed: command
        });
      });
    }
  }
}(this));

/* CmdEcho ----------------------------------------------------------------- */

(function(module) {
  /**
  * 'echo' command
  */
  gcli.addCommand({
    name: "echo",
    description: gcli.lookup("echoDesc"),
    params: [
      {
        name: "message",
        type: "string",
        description: gcli.lookup("echoMessageDesc")
      }
    ],
    returnType: "string",
    hidden: true,
    exec: function Command_echo(args, context) {
      return args.message;
    }
  });
}(this));

/* CmdExport --------------------------------------------------------------- */

(function(module) {
  /**
  * 'export' command
  */
  gcli.addCommand({
    name: "export",
    description: gcli.lookup("exportDesc"),
  });

  /**
  * The 'export html' command. This command allows the user to export the page to
  * HTML after they do DOM changes.
  */
  gcli.addCommand({
    name: "export html",
    description: gcli.lookup("exportHtmlDesc"),
    exec: function(args, context) {
      let html = context.environment.document.documentElement.outerHTML;
      let url = 'data:text/plain;charset=utf8,' + encodeURIComponent(html);
      context.environment.window.open(url);
    }
  });
}(this));

/* CmdJsb ------------------------------------------------------------------ */

(function(module) {
  const XMLHttpRequest =
    Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1");

  XPCOMUtils.defineLazyModuleGetter(this, "js_beautify",
                                    "resource:///modules/devtools/Jsbeautify.jsm");

  /**
  * jsb command.
  */
  gcli.addCommand({
    name: 'jsb',
    description: gcli.lookup('jsbDesc'),
    returnValue:'string',
    params: [
      {
        name: 'url',
        type: 'string',
        description: gcli.lookup('jsbUrlDesc')
      },
      {
        group: gcli.lookup("jsbOptionsDesc"),
        params: [
          {
            name: 'indentSize',
            type: 'number',
            description: gcli.lookup('jsbIndentSizeDesc'),
            manual: gcli.lookup('jsbIndentSizeManual'),
            defaultValue: 2
          },
          {
            name: 'indentChar',
            type: {
              name: 'selection',
              lookup: [
                { name: "space", value: " " },
                { name: "tab", value: "\t" }
              ]
            },
            description: gcli.lookup('jsbIndentCharDesc'),
            manual: gcli.lookup('jsbIndentCharManual'),
            defaultValue: ' ',
          },
          {
            name: 'doNotPreserveNewlines',
            type: 'boolean',
            description: gcli.lookup('jsbDoNotPreserveNewlinesDesc')
          },
          {
            name: 'preserveMaxNewlines',
            type: 'number',
            description: gcli.lookup('jsbPreserveMaxNewlinesDesc'),
            manual: gcli.lookup('jsbPreserveMaxNewlinesManual'),
            defaultValue: -1
          },
          {
            name: 'jslintHappy',
            type: 'boolean',
            description: gcli.lookup('jsbJslintHappyDesc'),
            manual: gcli.lookup('jsbJslintHappyManual')
          },
          {
            name: 'braceStyle',
            type: {
              name: 'selection',
              data: ['collapse', 'expand', 'end-expand', 'expand-strict']
            },
            description: gcli.lookup('jsbBraceStyleDesc2'),
            manual: gcli.lookup('jsbBraceStyleManual2'),
            defaultValue: "collapse"
          },
          {
            name: 'noSpaceBeforeConditional',
            type: 'boolean',
            description: gcli.lookup('jsbNoSpaceBeforeConditionalDesc')
          },
          {
            name: 'unescapeStrings',
            type: 'boolean',
            description: gcli.lookup('jsbUnescapeStringsDesc'),
            manual: gcli.lookup('jsbUnescapeStringsManual')
          }
        ]
      }
    ],
    exec: function(args, context) {
      let opts = {
        indent_size: args.indentSize,
        indent_char: args.indentChar,
        preserve_newlines: !args.doNotPreserveNewlines,
        max_preserve_newlines: args.preserveMaxNewlines == -1 ?
                              undefined : args.preserveMaxNewlines,
        jslint_happy: args.jslintHappy,
        brace_style: args.braceStyle,
        space_before_conditional: !args.noSpaceBeforeConditional,
        unescape_strings: args.unescapeStrings
      };

      let xhr = new XMLHttpRequest();

      try {
        xhr.open("GET", args.url, true);
      } catch(e) {
        return gcli.lookup('jsbInvalidURL');
      }

      let deferred = context.defer();

      xhr.onreadystatechange = function(aEvt) {
        if (xhr.readyState == 4) {
          if (xhr.status == 200 || xhr.status == 0) {
            let browserDoc = context.environment.chromeDocument;
            let browserWindow = browserDoc.defaultView;
            let gBrowser = browserWindow.gBrowser;
            let result = js_beautify(xhr.responseText, opts);

            browserWindow.Scratchpad.ScratchpadManager.openScratchpad({text: result});

            deferred.resolve();
          } else {
            deferred.resolve("Unable to load page to beautify: " + args.url + " " +
                             xhr.status + " " + xhr.statusText);
          }
        };
      }
      xhr.send(null);
      return deferred.promise;
    }
  });
}(this));

/* CmdPagemod -------------------------------------------------------------- */

(function(module) {
  /**
  * 'pagemod' command
  */
  gcli.addCommand({
    name: "pagemod",
    description: gcli.lookup("pagemodDesc"),
  });

  /**
  * The 'pagemod replace' command. This command allows the user to search and
  * replace within text nodes and attributes.
  */
  gcli.addCommand({
    name: "pagemod replace",
    description: gcli.lookup("pagemodReplaceDesc"),
    params: [
      {
        name: "search",
        type: "string",
        description: gcli.lookup("pagemodReplaceSearchDesc"),
      },
      {
        name: "replace",
        type: "string",
        description: gcli.lookup("pagemodReplaceReplaceDesc"),
      },
      {
        name: "ignoreCase",
        type: "boolean",
        description: gcli.lookup("pagemodReplaceIgnoreCaseDesc"),
      },
      {
        name: "selector",
        type: "string",
        description: gcli.lookup("pagemodReplaceSelectorDesc"),
        defaultValue: "*:not(script):not(style):not(embed):not(object):not(frame):not(iframe):not(frameset)",
      },
      {
        name: "root",
        type: "node",
        description: gcli.lookup("pagemodReplaceRootDesc"),
        defaultValue: null,
      },
      {
        name: "attrOnly",
        type: "boolean",
        description: gcli.lookup("pagemodReplaceAttrOnlyDesc"),
      },
      {
        name: "contentOnly",
        type: "boolean",
        description: gcli.lookup("pagemodReplaceContentOnlyDesc"),
      },
      {
        name: "attributes",
        type: "string",
        description: gcli.lookup("pagemodReplaceAttributesDesc"),
        defaultValue: null,
      },
    ],
    exec: function(args, context) {
      let document = context.environment.contentDocument;
      let searchTextNodes = !args.attrOnly;
      let searchAttributes = !args.contentOnly;
      let regexOptions = args.ignoreCase ? 'ig' : 'g';
      let search = new RegExp(escapeRegex(args.search), regexOptions);
      let attributeRegex = null;
      if (args.attributes) {
        attributeRegex = new RegExp(args.attributes, regexOptions);
      }

      let root = args.root || document;
      let elements = root.querySelectorAll(args.selector);
      elements = Array.prototype.slice.call(elements);

      let replacedTextNodes = 0;
      let replacedAttributes = 0;

      function replaceAttribute() {
        replacedAttributes++;
        return args.replace;
      }
      function replaceTextNode() {
        replacedTextNodes++;
        return args.replace;
      }

      for (let i = 0; i < elements.length; i++) {
        let element = elements[i];
        if (searchTextNodes) {
          for (let y = 0; y < element.childNodes.length; y++) {
            let node = element.childNodes[y];
            if (node.nodeType == node.TEXT_NODE) {
              node.textContent = node.textContent.replace(search, replaceTextNode);
            }
          }
        }

        if (searchAttributes) {
          if (!element.attributes) {
            continue;
          }
          for (let y = 0; y < element.attributes.length; y++) {
            let attr = element.attributes[y];
            if (!attributeRegex || attributeRegex.test(attr.name)) {
              attr.value = attr.value.replace(search, replaceAttribute);
            }
          }
        }
      }

      return gcli.lookupFormat("pagemodReplaceResult",
                              [elements.length, replacedTextNodes,
                                replacedAttributes]);
    }
  });

  /**
  * 'pagemod remove' command
  */
  gcli.addCommand({
    name: "pagemod remove",
    description: gcli.lookup("pagemodRemoveDesc"),
  });


  /**
  * The 'pagemod remove element' command.
  */
  gcli.addCommand({
    name: "pagemod remove element",
    description: gcli.lookup("pagemodRemoveElementDesc"),
    params: [
      {
        name: "search",
        type: "string",
        description: gcli.lookup("pagemodRemoveElementSearchDesc"),
      },
      {
        name: "root",
        type: "node",
        description: gcli.lookup("pagemodRemoveElementRootDesc"),
        defaultValue: null,
      },
      {
        name: 'stripOnly',
        type: 'boolean',
        description: gcli.lookup("pagemodRemoveElementStripOnlyDesc"),
      },
      {
        name: 'ifEmptyOnly',
        type: 'boolean',
        description: gcli.lookup("pagemodRemoveElementIfEmptyOnlyDesc"),
      },
    ],
    exec: function(args, context) {
      let document = context.environment.contentDocument;
      let root = args.root || document;
      let elements = Array.prototype.slice.call(root.querySelectorAll(args.search));

      let removed = 0;
      for (let i = 0; i < elements.length; i++) {
        let element = elements[i];
        let parentNode = element.parentNode;
        if (!parentNode || !element.removeChild) {
          continue;
        }
        if (args.stripOnly) {
          while (element.hasChildNodes()) {
            parentNode.insertBefore(element.childNodes[0], element);
          }
        }
        if (!args.ifEmptyOnly || !element.hasChildNodes()) {
          element.parentNode.removeChild(element);
          removed++;
        }
      }

      return gcli.lookupFormat("pagemodRemoveElementResultMatchedAndRemovedElements",
                              [elements.length, removed]);
    }
  });

  /**
  * The 'pagemod remove attribute' command.
  */
  gcli.addCommand({
    name: "pagemod remove attribute",
    description: gcli.lookup("pagemodRemoveAttributeDesc"),
    params: [
      {
        name: "searchAttributes",
        type: "string",
        description: gcli.lookup("pagemodRemoveAttributeSearchAttributesDesc"),
      },
      {
        name: "searchElements",
        type: "string",
        description: gcli.lookup("pagemodRemoveAttributeSearchElementsDesc"),
      },
      {
        name: "root",
        type: "node",
        description: gcli.lookup("pagemodRemoveAttributeRootDesc"),
        defaultValue: null,
      },
      {
        name: "ignoreCase",
        type: "boolean",
        description: gcli.lookup("pagemodRemoveAttributeIgnoreCaseDesc"),
      },
    ],
    exec: function(args, context) {
      let document = context.environment.contentDocument;

      let root = args.root || document;
      let regexOptions = args.ignoreCase ? 'ig' : 'g';
      let attributeRegex = new RegExp(args.searchAttributes, regexOptions);
      let elements = root.querySelectorAll(args.searchElements);
      elements = Array.prototype.slice.call(elements);

      let removed = 0;
      for (let i = 0; i < elements.length; i++) {
        let element = elements[i];
        if (!element.attributes) {
          continue;
        }

        var attrs = Array.prototype.slice.call(element.attributes);
        for (let y = 0; y < attrs.length; y++) {
          let attr = attrs[y];
          if (attributeRegex.test(attr.name)) {
            element.removeAttribute(attr.name);
            removed++;
          }
        }
      }

      return gcli.lookupFormat("pagemodRemoveAttributeResult",
                              [elements.length, removed]);
    }
  });

  /**
  * Make a given string safe to use  in a regular expression.
  *
  * @param string aString
  *        The string you want to use in a regex.
  * @return string
  *         The equivalent of |aString| but safe to use in a regex.
  */
  function escapeRegex(aString) {
    return aString.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
  }
}(this));

/* CmdTools -------------------------------------------------------------- */

(function(module) {
  gcli.addCommand({
    name: "tools",
    description: gcli.lookup("toolsDesc"),
    manual: gcli.lookup("toolsManual"),
    get hidden() gcli.hiddenByChromePref(),
  });

  gcli.addCommand({
    name: "tools srcdir",
    description: gcli.lookup("toolsSrcdirDesc"),
    manual: gcli.lookup("toolsSrcdirManual"),
    get hidden() gcli.hiddenByChromePref(),
    params: [
      {
        name: "srcdir",
        type: "string",
        description: gcli.lookup("toolsSrcdirDir")
      }
    ],
    returnType: "string",
    exec: function(args, context) {
      let promise = context.createPromise();
      let existsPromise = OS.File.exists(args.srcdir + "/CLOBBER");
      existsPromise.then(function(exists) {
        if (exists) {
          var str = Cc["@mozilla.org/supports-string;1"]
            .createInstance(Ci.nsISupportsString);
          str.data = args.srcdir;
          Services.prefs.setComplexValue("devtools.loader.srcdir",
              Components.interfaces.nsISupportsString, str);
          devtools.reload();
          promise.resolve(gcli.lookupFormat("toolsSrcdirReloaded", [args.srcdir]));
          return;
        }
        promise.reject(gcli.lookupFormat("toolsSrcdirNotFound", [args.srcdir]));
      });
      return promise;
    }
  });

  gcli.addCommand({
    name: "tools builtin",
    description: gcli.lookup("toolsBuiltinDesc"),
    manual: gcli.lookup("toolsBuiltinManual"),
    get hidden() gcli.hiddenByChromePref(),
    returnType: "string",
    exec: function(args, context) {
      Services.prefs.clearUserPref("devtools.loader.srcdir");
      devtools.reload();
      return gcli.lookup("toolsBuiltinReloaded");
    }
  });

  gcli.addCommand({
    name: "tools reload",
    description: gcli.lookup("toolsReloadDesc"),
    get hidden() gcli.hiddenByChromePref() || !Services.prefs.prefHasUserValue("devtools.loader.srcdir"),

    returnType: "string",
    exec: function(args, context) {
      devtools.reload();
      return gcli.lookup("toolsReloaded");
    }
  });
}(this));

/* CmdRestart -------------------------------------------------------------- */

(function(module) {
  /**
  * Restart command
  *
  * @param boolean nocache
  *        Disables loading content from cache upon restart.
  *
  * Examples :
  * >> restart
  * - restarts browser immediately
  * >> restart --nocache
  * - restarts immediately and starts Firefox without using cache
  */

  gcli.addCommand({
    name: "restart",
    description: gcli.lookupFormat("restartBrowserDesc", [BRAND_SHORT_NAME]),
    params: [
      {
        name: "nocache",
        type: "boolean",
        description: gcli.lookup("restartBrowserNocacheDesc")
      }
    ],
    returnType: "string",
    exec: function Restart(args, context) {
      let canceled = Cc["@mozilla.org/supports-PRBool;1"]
                      .createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(canceled, "quit-application-requested", "restart");
      if (canceled.data) {
        return gcli.lookup("restartBrowserRequestCancelled");
      }

      // disable loading content from cache.
      if (args.nocache) {
        Services.appinfo.invalidateCachesOnRestart();
      }

      // restart
      Cc['@mozilla.org/toolkit/app-startup;1']
        .getService(Ci.nsIAppStartup)
        .quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
      return gcli.lookupFormat("restartBrowserRestarting", [BRAND_SHORT_NAME]);
    }
  });
}(this));

/* CmdScreenshot ----------------------------------------------------------- */

(function(module) {
  XPCOMUtils.defineLazyModuleGetter(this, "LayoutHelpers",
                                    "resource:///modules/devtools/LayoutHelpers.jsm");

  // String used as an indication to generate default file name in the following
  // format: "Screen Shot yyyy-mm-dd at HH.MM.SS.png"
  const FILENAME_DEFAULT_VALUE = " ";

  /**
  * 'screenshot' command
  */
  gcli.addCommand({
    name: "screenshot",
    description: gcli.lookup("screenshotDesc"),
    manual: gcli.lookup("screenshotManual"),
    returnType: "html",
    params: [
      {
        name: "filename",
        type: "string",
        defaultValue: FILENAME_DEFAULT_VALUE,
        description: gcli.lookup("screenshotFilenameDesc"),
        manual: gcli.lookup("screenshotFilenameManual")
      },
      {
        group: gcli.lookup("screenshotGroupOptions"),
        params: [
          {
            name: "clipboard",
            type: "boolean",
            description: gcli.lookup("screenshotClipboardDesc"),
            manual: gcli.lookup("screenshotClipboardManual")
          },
          {
            name: "chrome",
            type: "boolean",
            description: gcli.lookupFormat("screenshotChromeDesc2", [BRAND_SHORT_NAME]),
            manual: gcli.lookupFormat("screenshotChromeManual2", [BRAND_SHORT_NAME])
          },
          {
            name: "delay",
            type: { name: "number", min: 0 },
            defaultValue: 0,
            description: gcli.lookup("screenshotDelayDesc"),
            manual: gcli.lookup("screenshotDelayManual")
          },
          {
            name: "fullpage",
            type: "boolean",
            description: gcli.lookup("screenshotFullPageDesc"),
            manual: gcli.lookup("screenshotFullPageManual")
          },
          {
            name: "selector",
            type: "node",
            defaultValue: null,
            description: gcli.lookup("inspectNodeDesc"),
            manual: gcli.lookup("inspectNodeManual")
          }
        ]
      }
    ],
    exec: function Command_screenshot(args, context) {
      if (args.chrome && args.selector) {
        // Node screenshot with chrome option does not work as inteded
        // Refer https://bugzilla.mozilla.org/show_bug.cgi?id=659268#c7
        // throwing for now.
        throw new Error(gcli.lookup("screenshotSelectorChromeConflict"));
      }
      var document = args.chrome? context.environment.chromeDocument
                                : context.environment.contentDocument;
      if (args.delay > 0) {
        var deferred = context.defer();
        document.defaultView.setTimeout(function Command_screenshotDelay() {
          let reply = this.grabScreen(document, args.filename, args.clipboard,
                                      args.fullpage);
          deferred.resolve(reply);
        }.bind(this), args.delay * 1000);
        return deferred.promise;
      }
      else {
        return this.grabScreen(document, args.filename, args.clipboard,
                              args.fullpage, args.selector);
      }
    },
    grabScreen:
    function Command_screenshotGrabScreen(document, filename, clipboard,
                                          fullpage, node) {
      let window = document.defaultView;
      let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
      let left = 0;
      let top = 0;
      let width;
      let height;
      let div = document.createElementNS("http://www.w3.org/1999/xhtml", "div");

      if (!fullpage) {
        if (!node) {
          left = window.scrollX;
          top = window.scrollY;
          width = window.innerWidth;
          height = window.innerHeight;
        } else {
          let rect = LayoutHelpers.getRect(node, window);
          top = rect.top;
          left = rect.left;
          width = rect.width;
          height = rect.height;
        }
      } else {
        width = window.innerWidth + window.scrollMaxX;
        height = window.innerHeight + window.scrollMaxY;
      }
      canvas.width = width;
      canvas.height = height;

      let ctx = canvas.getContext("2d");
      ctx.drawWindow(window, left, top, width, height, "#fff");
      let data = canvas.toDataURL("image/png", "");

      let loadContext = document.defaultView
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsILoadContext);

      try {
        if (clipboard) {
          let io = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);
          let channel = io.newChannel(data, null, null);
          let input = channel.open();
          let imgTools = Cc["@mozilla.org/image/tools;1"]
                          .getService(Ci.imgITools);

          let container = {};
          imgTools.decodeImageData(input, channel.contentType, container);

          let wrapped = Cc["@mozilla.org/supports-interface-pointer;1"]
                          .createInstance(Ci.nsISupportsInterfacePointer);
          wrapped.data = container.value;

          let trans = Cc["@mozilla.org/widget/transferable;1"]
                        .createInstance(Ci.nsITransferable);
          trans.init(loadContext);
          trans.addDataFlavor(channel.contentType);
          trans.setTransferData(channel.contentType, wrapped, -1);

          let clipid = Ci.nsIClipboard;
          let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(clipid);
          clip.setData(trans, null, clipid.kGlobalClipboard);
          div.textContent = gcli.lookup("screenshotCopied");
          return div;
        }
      }
      catch (ex) {
        div.textContent = gcli.lookup("screenshotErrorCopying");
        return div;
      }

      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);

      // Create a name for the file if not present
      if (filename == FILENAME_DEFAULT_VALUE) {
        let date = new Date();
        let dateString = date.getFullYear() + "-" + (date.getMonth() + 1) +
                        "-" + date.getDate();
        dateString = dateString.split("-").map(function(part) {
          if (part.length == 1) {
            part = "0" + part;
          }
          return part;
        }).join("-");
        let timeString = date.toTimeString().replace(/:/g, ".").split(" ")[0];
        filename = gcli.lookupFormat("screenshotGeneratedFilename",
                                    [dateString, timeString]) + ".png";
      }
      // Check there is a .png extension to filename
      else if (!filename.match(/.png$/i)) {
        filename += ".png";
      }

      // If the filename is relative, tack it onto the download directory
      if (!filename.match(/[\\\/]/)) {
        let downloadMgr = Cc["@mozilla.org/download-manager;1"]
                            .getService(Ci.nsIDownloadManager);
        let tempfile = downloadMgr.userDownloadsDirectory;
        tempfile.append(filename);
        filename = tempfile.path;
      }

      try {
        file.initWithPath(filename);
      } catch (ex) {
        div.textContent = gcli.lookup("screenshotErrorSavingToFile") + " " + filename;
        return div;
      }

      let ioService = Cc["@mozilla.org/network/io-service;1"]
                        .getService(Ci.nsIIOService);

      let Persist = Ci.nsIWebBrowserPersist;
      let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                      .createInstance(Persist);
      persist.persistFlags = Persist.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                            Persist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

      let source = ioService.newURI(data, "UTF8", null);
      persist.saveURI(source, null, null, null, null, file, loadContext);

      div.textContent = gcli.lookup("screenshotSavedToFile") + " \"" + filename +
                        "\"";
      div.addEventListener("click", function openFile() {
        div.removeEventListener("click", openFile);
        file.reveal();
      });
      div.style.cursor = "pointer";
      let image = document.createElement("div");
      let previewHeight = parseInt(256*height/width);
      image.setAttribute("style",
                        "width:256px; height:" + previewHeight + "px;" +
                        "max-height: 256px;" +
                        "background-image: url('" + data + "');" +
                        "background-size: 256px " + previewHeight + "px;" +
                        "margin: 4px; display: block");
      div.appendChild(image);
      return div;
    }
  });
}(this));

/* CmdPaintFlashing ------------------------------------------------------- */

(function(module) {
  /**
  * 'paintflashing' command
  */

  gcli.addCommand({
    name: 'paintflashing',
    description: gcli.lookup('paintflashingDesc')
  });

  gcli.addCommand({
    name: 'paintflashing on',
    description: gcli.lookup('paintflashingOnDesc'),
    manual: gcli.lookup('paintflashingManual'),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() gcli.hiddenByChromePref(),
          description: gcli.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function(args, context) {
      var window;
      if (args.chrome) {
        window = context.environment.chromeDocument.defaultView;
      } else {
        window = context.environment.contentDocument.defaultView;
      }
      window.QueryInterface(Ci.nsIInterfaceRequestor).
             getInterface(Ci.nsIDOMWindowUtils).
             paintFlashing = true;
      onPaintFlashingChanged(context);
    }
  });

  gcli.addCommand({
    name: 'paintflashing off',
    description: gcli.lookup('paintflashingOffDesc'),
    manual: gcli.lookup('paintflashingManual'),
    params: [{
      group: "options",
      params: [
        {
          type: "boolean",
          name: "chrome",
          get hidden() gcli.hiddenByChromePref(),
          description: gcli.lookup("paintflashingChromeDesc"),
        }
      ]
    }],
    exec: function(args, context) {
      if (args.chrome) {
        var window = context.environment.chromeDocument.defaultView;
      } else {
        var window = context.environment.contentDocument.defaultView;
      }
      window.QueryInterface(Ci.nsIInterfaceRequestor).
             getInterface(Ci.nsIDOMWindowUtils).
             paintFlashing = false;
      onPaintFlashingChanged(context);
    }
  });

  gcli.addCommand({
    name: 'paintflashing toggle',
    hidden: true,
    buttonId: "command-button-paintflashing",
    buttonClass: "command-button",
    state: {
      isChecked: function(aTarget) {
        if (aTarget.isLocalTab) {
          let window = aTarget.tab.linkedBrowser.contentWindow;
          let wUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).
                              getInterface(Ci.nsIDOMWindowUtils);
          return wUtils.paintFlashing;
        } else {
          throw new Error("Unsupported target");
        }
      },
      onChange: function(aTarget, aChangeHandler) {
        eventEmitter.on("changed", aChangeHandler);
      },
      offChange: function(aTarget, aChangeHandler) {
        eventEmitter.off("changed", aChangeHandler);
      },
    },
    tooltipText: gcli.lookup("paintflashingTooltip"),
    description: gcli.lookup('paintflashingToggleDesc'),
    manual: gcli.lookup('paintflashingManual'),
    exec: function(args, context) {
      var gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
      var window = gBrowser.contentWindow;
      var wUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIDOMWindowUtils);
      wUtils.paintFlashing = !wUtils.paintFlashing;
      onPaintFlashingChanged(context);
    }
  });

  let eventEmitter = new EventEmitter();
  function onPaintFlashingChanged(context) {
    var gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
    var tab = gBrowser.selectedTab;
    eventEmitter.emit("changed", tab);
    function fireChange() {
      eventEmitter.emit("changed", tab);
    }
    var target = devtools.TargetFactory.forTab(tab);
    target.off("navigate", fireChange);
    target.once("navigate", fireChange);
  }
}(this));

/* CmdAppCache ------------------------------------------------------- */

(function(module) {
  /**
  * 'appcache' command
  */

  gcli.addCommand({
    name: 'appcache',
    description: gcli.lookup('appCacheDesc')
  });

  gcli.addConverter({
    from: "appcacheerrors",
    to: "view",
    exec: function([errors, manifestURI], context) {
      if (errors.length == 0) {
        return context.createView({
          html: "<span>" + gcli.lookup("appCacheValidatedSuccessfully") + "</span>"
        });
      }

      let appcacheValidateHtml =
        "<h4>Manifest URI: ${manifestURI}</h4>" +
        "<ol>" +
        "  <li foreach='error in ${errors}'>" +
        "    ${error.msg}" +
        "  </li>" +
        "</ol>";

      return context.createView({
        html: "<div>" + appcacheValidateHtml + "</div>",
        data: {
          errors: errors,
          manifestURI: manifestURI
        }
      });
    }
  });

  gcli.addCommand({
    name: 'appcache validate',
    description: gcli.lookup('appCacheValidateDesc'),
    manual: gcli.lookup('appCacheValidateManual'),
    returnType: 'appcacheerrors',
    params: [{
      group: "options",
      params: [
        {
          type: "string",
          name: "uri",
          description: gcli.lookup("appCacheValidateUriDesc"),
          defaultValue: null,
        }
      ]
    }],
    exec: function(args, context) {
      let utils;
      let promise = context.createPromise();

      if (args.uri) {
        utils = new AppCacheUtils(args.uri);
      } else {
        let doc = context.environment.contentDocument;
        utils = new AppCacheUtils(doc);
      }

      utils.validateManifest().then(function(errors) {
        promise.resolve([errors, utils.manifestURI || "-"]);
      });

      return promise;
    }
  });

  gcli.addCommand({
    name: 'appcache clear',
    description: gcli.lookup('appCacheClearDesc'),
    manual: gcli.lookup('appCacheClearManual'),
    exec: function(args, context) {
      let utils = new AppCacheUtils(args.uri);
      utils.clearAll();

      return gcli.lookup("appCacheClearCleared");
    }
  });

  gcli.addConverter({
    from: "appcacheentries",
    to: "view",
    exec: function(entries, context) {
      if (!entries) {
        return context.createView({
          html: "<span>" + gcli.lookup("appCacheManifestContainsErrors") + "</span>"
        });
      }

      if (entries.length == 0) {
        return context.createView({
          html: "<span>" + gcli.lookup("appCacheNoResults") + "</span>"
        });
      }

      let appcacheListEntries = "" +
        "<ul class='gcli-appcache-list'>" +
        "  <li foreach='entry in ${entries}'>" +
        "    <table class='gcli-appcache-detail'>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListKey") + "</td>" +
        "        <td>${entry.key}</td>" +
        "      </tr>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListFetchCount") + "</td>" +
        "        <td>${entry.fetchCount}</td>" +
        "      </tr>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListLastFetched") + "</td>" +
        "        <td>${entry.lastFetched}</td>" +
        "      </tr>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListLastModified") + "</td>" +
        "        <td>${entry.lastModified}</td>" +
        "      </tr>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListExpirationTime") + "</td>" +
        "        <td>${entry.expirationTime}</td>" +
        "      </tr>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListDataSize") + "</td>" +
        "        <td>${entry.dataSize}</td>" +
        "      </tr>" +
        "      <tr>" +
        "        <td>" + gcli.lookup("appCacheListDeviceID") + "</td>" +
        "        <td>${entry.deviceID} <span class='gcli-out-shortcut' " +
        "onclick='${onclick}' ondblclick='${ondblclick}' " +
        "data-command='appcache viewentry ${entry.key}'" +
        ">" + gcli.lookup("appCacheListViewEntry") + "</span>" +
        "        </td>" +
        "      </tr>" +
        "    </table>" +
        "  </li>" +
        "</ul>";

      return context.createView({
        html: appcacheListEntries,
        data: {
          entries: entries,
          onclick: createUpdateHandler(context),
          ondblclick: createExecuteHandler(context),
        }
      });
    }
  });

  gcli.addCommand({
    name: 'appcache list',
    description: gcli.lookup('appCacheListDesc'),
    manual: gcli.lookup('appCacheListManual'),
    returnType: "appcacheentries",
    params: [{
      group: "options",
      params: [
        {
          type: "string",
          name: "search",
          description: gcli.lookup("appCacheListSearchDesc"),
          defaultValue: null,
        },
      ]
    }],
    exec: function(args, context) {
      let doc = context.environment.contentDocument;
      let utils = new AppCacheUtils();

      let entries = utils.listEntries(args.search);
      return entries;
    }
  });

  gcli.addCommand({
    name: 'appcache viewentry',
    description: gcli.lookup('appCacheViewEntryDesc'),
    manual: gcli.lookup('appCacheViewEntryManual'),
    params: [
      {
        type: "string",
        name: "key",
        description: gcli.lookup("appCacheViewEntryKey"),
        defaultValue: null,
      }
    ],
    exec: function(args, context) {
      let doc = context.environment.contentDocument;
      let utils = new AppCacheUtils();

      let result = utils.viewEntry(args.key);
      if (result) {
        return result;
      }
    }
  });

  /**
   * Helper to find the 'data-command' attribute and call some action on it.
   * @see |updateCommand()| and |executeCommand()|
   */
  function withCommand(element, action) {
    let command = element.getAttribute("data-command");
    if (!command) {
      command = element.querySelector("*[data-command]")
              .getAttribute("data-command");
    }

    if (command) {
      action(command);
    }
    else {
      console.warn("Missing data-command for " + util.findCssSelector(element));
    }
  }

  /**
   * Create a handler to update the requisition to contain the text held in the
   * first matching data-command attribute under the currentTarget of the event.
   * @param context Either a Requisition or an ExecutionContext or another object
   * that contains an |update()| function that follows a similar contract.
   */
  function createUpdateHandler(context) {
    return function(ev) {
      withCommand(ev.currentTarget, function(command) {
        context.update(command);
      });
    }
  }

  /**
   * Create a handler to execute the text held in the data-command attribute
   * under the currentTarget of the event.
   * @param context Either a Requisition or an ExecutionContext or another object
   * that contains an |update()| function that follows a similar contract.
   */
  function createExecuteHandler(context) {
    return function(ev) {
      withCommand(ev.currentTarget, function(command) {
        context.exec({
          visible: true,
          typed: command
        });
      });
    }
  }
}(this));
