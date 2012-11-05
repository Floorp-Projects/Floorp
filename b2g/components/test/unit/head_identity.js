/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const Ci = Components.interfaces;
const Cu = Components.utils;

// The following boilerplate makes sure that XPCom calls
// that use the profile directory work.

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MinimalIDService",
                                  "resource://gre/modules/identity/MinimalIdentity.jsm",
                                  "IdentityService");

XPCOMUtils.defineLazyModuleGetter(this,
                                  "Logger",
                                  "resource://gre/modules/identity/LogUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "uuidGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const TEST_URL = "https://myfavoriteflan.com";
const TEST_USER = "uumellmahaye1969@hotmail.com";
const TEST_PRIVKEY = "i-am-a-secret";
const TEST_CERT = "i~like~pie";

// The following are utility functions for Identity testing

function log(...aMessageArgs) {
  Logger.log.apply(Logger, ["test"].concat(aMessageArgs));
}

function partial(fn) {
  let args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return fn.apply(this, args.concat(Array.prototype.slice.call(arguments)));
  };
}

function uuid() {
  return uuidGenerator.generateUUID().toString();
}

// create a mock "doc" object, which the Identity Service
// uses as a pointer back into the doc object
function mockDoc(aIdentity, aOrigin, aDoFunc) {
  let mockedDoc = {};
  mockedDoc.id = uuid();
  mockedDoc.loggedInUser = aIdentity;
  mockedDoc.origin = aOrigin;
  mockedDoc['do'] = aDoFunc;
  mockedDoc.doReady = partial(aDoFunc, 'ready');
  mockedDoc.doLogin = partial(aDoFunc, 'login');
  mockedDoc.doLogout = partial(aDoFunc, 'logout');
  mockedDoc.doError = partial(aDoFunc, 'error');
  mockedDoc.doCancel = partial(aDoFunc, 'cancel');
  mockedDoc.doCoffee = partial(aDoFunc, 'coffee');

  return mockedDoc;
}

// create a mock "pipe" object that would normally communicate
// messages up to gaia (either the trusty ui or the hidden iframe),
// and convey messages back down from gaia to the controller through
// the message callback.
function mockPipe() {
  let MockedPipe = {
    communicate: function(aRpOptions, aGaiaOptions, aMessageCallback) {
      switch (aGaiaOptions.message) {
        case "identity-delegate-watch":
          aMessageCallback({json: {method: "ready"}});
          break;
        case "identity-delegate-request":
          aMessageCallback({json: {method: "login", assertion: TEST_CERT}});
          break;
        case "identity-delegate-logout":
          aMessageCallback({json: {method: "logout"}});
          break;
        default:
          throw("what the what?? " + aGaiaOptions.message);
          break;
      }
    }
  };
  return MockedPipe;
}

// mimicking callback funtionality for ease of testing
// this observer auto-removes itself after the observe function
// is called, so this is meant to observe only ONE event.
function makeObserver(aObserveTopic, aObserveFunc) {
  let observer = {
    // nsISupports provides type management in C++
    // nsIObserver is to be an observer
    QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

    observe: function (aSubject, aTopic, aData) {
      if (aTopic == aObserveTopic) {
        Services.obs.removeObserver(observer, aObserveTopic);
        aObserveFunc(aSubject, aTopic, aData);
      }
    }
  };

  Services.obs.addObserver(observer, aObserveTopic, false);
}

// a hook to set up the ID service with an identity with keypair and all
// when ready, invoke callback with the identity.  It's there if we need it.
function setup_test_identity(identity, cert, cb) {
  cb();
}

// takes a list of functions and returns a function that
// when called the first time, calls the first func,
// then the next time the second, etc.
function call_sequentially() {
  let numCalls = 0;
  let funcs = arguments;

  return function() {
    if (!funcs[numCalls]) {
      let argString = Array.prototype.slice.call(arguments).join(",");
      do_throw("Too many calls: " + argString);
      return;
    }
    funcs[numCalls].apply(funcs[numCalls],arguments);
    numCalls += 1;
  };
}
