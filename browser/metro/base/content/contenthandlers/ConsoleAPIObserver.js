/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dump("### ConsoleAPIObserver.js loaded\n");

/*
 * ConsoleAPIObserver
 *
 */ 

var ConsoleAPIObserver = {
  init: function init() {
    addMessageListener("Browser:TabOpen", this);
    addMessageListener("Browser:TabClose", this);
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:TabOpen":
        Services.obs.addObserver(this, "console-api-log-event", false);
        break;
      case "Browser:TabClose":
        Services.obs.removeObserver(this, "console-api-log-event");
        break;
    }
  },

  observe: function observe(aMessage, aTopic, aData) {
    let contentWindowId = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    aMessage = aMessage.wrappedJSObject;
    if (aMessage.ID != contentWindowId)
      return;

    let mappedArguments = Array.map(aMessage.arguments, this.formatResult, this);
    let joinedArguments = Array.join(mappedArguments, " ");

    if (aMessage.level == "error" || aMessage.level == "warn") {
      let flag = (aMessage.level == "error" ? Ci.nsIScriptError.errorFlag : Ci.nsIScriptError.warningFlag);
      let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(Ci.nsIScriptError);
      consoleMsg.init(joinedArguments, null, null, 0, 0, flag, "content javascript");
      Services.console.logMessage(consoleMsg);
    } else if (aMessage.level == "trace") {
      let bundle = Services.strings.createBundle("chrome://global/locale/headsUpDisplay.properties");
      let args = aMessage.arguments;
      let filename = this.abbreviateSourceURL(args[0].filename);
      let functionName = args[0].functionName || bundle.GetStringFromName("stacktrace.anonymousFunction");
      let lineNumber = args[0].lineNumber;

      let body = bundle.formatStringFromName("stacktrace.outputMessage", [filename, functionName, lineNumber], 3);
      body += "\n";
      args.forEach(function(aFrame) {
        body += aFrame.filename + " :: " + aFrame.functionName + " :: " + aFrame.lineNumber + "\n";
      });

      Services.console.logStringMessage(body);
    } else {
      Services.console.logStringMessage(joinedArguments);
    }
  },

  getResultType: function getResultType(aResult) {
    let type = aResult === null ? "null" : typeof aResult;
    if (type == "object" && aResult.constructor && aResult.constructor.name)
      type = aResult.constructor.name;
    return type.toLowerCase();
  },

  formatResult: function formatResult(aResult) {
    let output = "";
    let type = this.getResultType(aResult);
    switch (type) {
      case "string":
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        output = aResult.toString();
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        if (aResult.toSource) {
          try {
            output = aResult.toSource();
          } catch (ex) { }
        }
        if (!output || output == "({})") {
          output = aResult.toString();
        }
        break;
    }

    return output;
  },

  abbreviateSourceURL: function abbreviateSourceURL(aSourceURL) {
    // Remove any query parameters.
    let hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1)
      aSourceURL = aSourceURL.substring(0, hookIndex);

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/")
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);

    // Remove all but the last path component.
    let slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1)
      aSourceURL = aSourceURL.substring(slashIndex + 1);

    return aSourceURL;
  }
};

ConsoleAPIObserver.init();

