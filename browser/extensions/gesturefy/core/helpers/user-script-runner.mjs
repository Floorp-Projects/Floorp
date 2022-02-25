/**
 * User Script Controller
 * helper to safely execute custom user scripts in the page context
 * will hopefully one day be replaced with https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/userScripts
 **/

//  This script is using content script specific functions like cloneInto so it should only be run as a content script (thus not in the installation page)
if (typeof cloneInto !== "function") console.warn("User scripts are disabled on this page.");

else {

// add the message event listener
browser.runtime.onMessage.addListener(handleMessage);


/**
 * Handles user script execution messages from the user script command
 **/

// Do not use an async method here because this would block every other message listener.
// Instead promises are returned when appropriate.
// See: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/Runtime/onMessage
function handleMessage (message, sender, sendResponse) {
  if (message.subject === "executeUserScript") {
    // create function in page script scope (not content script scope)
    // so it is not executed as privileged extension code and thus has no access to webextension apis
    // this also prevents interference with the extension code
    try {
      const executeUserScript = new window.wrappedJSObject.Function("TARGET", "API", message.data);
      const returnValue = executeUserScript(window.TARGET, API);
      // interpret every value even undefined or 0 as true (success) and only an explicit false as failure
      return Promise.resolve(returnValue != false);
    }
    // catch CSP errors that prevent the user script from running
    // this allows to declare a fallback command via the multi purpose command
    catch(error) {
      return Promise.resolve(false);
    }
  }
}


/**
 * Build API function object
 **/
const API = cloneInto({
  tabs: {
    query: apiFunctionCallHandler.bind(null, "tabs", "query"),
    create: apiFunctionCallHandler.bind(null, "tabs", "create"),
    remove: apiFunctionCallHandler.bind(null, "tabs", "remove"),
    update: apiFunctionCallHandler.bind(null, "tabs", "update"),
    move: apiFunctionCallHandler.bind(null, "tabs", "move")
  },
  windows: {
    get: apiFunctionCallHandler.bind(null, "windows", "get"),
    getCurrent: apiFunctionCallHandler.bind(null, "windows", "getCurrent"),
    create: apiFunctionCallHandler.bind(null, "windows", "create"),
    remove: apiFunctionCallHandler.bind(null, "windows", "remove"),
    update: apiFunctionCallHandler.bind(null, "windows", "update"),
  }
}, window.wrappedJSObject, { cloneFunctions: true });


/**
 * Forwards function calls to the background script and returns their values
 **/
function apiFunctionCallHandler (nameSpace, functionName, ...args) {
  return new window.Promise((resolve, reject) => {
    const value = browser.runtime.sendMessage({
      subject: "backgroundScriptAPICall",
      data: {
        "nameSpace": nameSpace,
        "functionName": functionName,
        "parameter": args
      }
    });
    value.then((value) => {
      resolve(cloneInto(value, window.wrappedJSObject));
    });
    value.catch((reason) => {
      reject(reason);
    });
  });
}

}