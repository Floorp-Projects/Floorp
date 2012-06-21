/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var SERVERS = {
  "_primary":"http://127.0.0.1:8088",
  "super_crazy":"http://www.example.com:80/chrome/dom/tests/mochitest/webapps/apps/super_crazy.webapp",
  "wild_crazy":"http://www.example.com:80/chrome/dom/tests/mochitest/webapps/apps/wild_crazy.webapp",
  "app_with_simple_service":"http://127.0.0.1:8888/tests/dom/tests/mochitest/webapps/servers/app_with_simple_service",
  "bad_content_type":"http://test2.example.org:80/chrome/dom/tests/mochitest/webapps/apps/bad_content_type.webapp",
  "json_syntax_error":"http://sub1.test1.example.org:80/chrome/dom/tests/mochitest/webapps/apps/json_syntax_error.webapp",
  "manifest_with_bom":"http://sub1.test2.example.org:80/chrome/dom/tests/mochitest/webapps/apps/manifest_with_bom.webapp",
  "missing_required_field":"http://sub2.test1.example.org:80/chrome/dom/tests/mochitest/webapps/apps/missing_required_field.webapp",
  "no_delegated_install":"http://sub2.test2.example.org:80/chrome/dom/tests/mochitest/webapps/apps/no_delegated_install.webapp"
 };

popup_listener();

var DEBUG = false;

/**
 * Wraps objects from the chrome context, and allows them to be accessed via the iframe
 * @name  The id of the iframe that is being loaded
 * @check An abstraction over ok / todo to allow for that determination to be made by the invoking code
 * @next  The next operation to jump to, this might need to be invoked by the iframe when the test has completed
 */

function onIframeLoad(name, check, next) {
  document.getElementById(name).contentWindow.wrappedJSObject.next = next;
  document.getElementById(name).contentWindow.wrappedJSObject.getOrigin = getOrigin;
  document.getElementById(name).contentWindow.wrappedJSObject.check = check;
  document.getElementById(name).contentWindow.wrappedJSObject.debug = debug;
  document.getElementById(name).contentWindow.wrappedJSObject.appURL = SERVERS[name]; 
  document.getElementById(name).contentWindow.wrappedJSObject.popup_listener = popup_listener;
  document.getElementById(name).contentWindow.wrappedJSObject.readFile = readFile;
}

/**
 * Uninstall All uninstalls all Apps
 * @next  The next operation to jump to, this might need to be invoked by the iframe when the test has completed
 */

function uninstallAll(next) {
  var pendingGetAll = navigator.mozApps.mgmt.getAll();
  pendingGetAll.onsuccess = function() {
    var m = this.result;
    var total = m.length;
    var finished = (total === 0);
    debug("total = " + total);
    for (var i=0; i < m.length; i++) {
      var app = m[i];
      var pendingUninstall = app.uninstall();
      pendingUninstall.onsuccess = function(r) {
        finished = (--total === 0);
        if(finished == true) {
          next();
        }
      };
      pendingUninstall.onerror = function () {
        finished = true;
        throw('Failed');
        if(finished == true) {
          next();
        }
      };
    }
    if(finished == true && total ==  0) {
      next();
    }
  }
}

function subsetOf(resultObj, list) {
  var returnObj = {} ;
  for (var i=0; i < list.length; i++) {
    returnObj[list[i]] = resultObj[list[i]];
  }
  return returnObj;
}

/**
 * Uninstall uninstalls a specific App
 * @appURL The manifest app url
 * @check An abstraction over ok / todo to allow for that determination to be made by the invoking code
 * @next  The next operation to jump to, this might need to be invoked by the iframe when the test has completed
 */

function uninstall(appURL, check, next) {
  var found = false;
  var finished = false;
  var pending = navigator.mozApps.getInstalled(); 
  pending.onsuccess = function () {
    var m = this.result;
    for (var i=0; i < m.length; i++) {
      var app = m[i];
      if (app.manifestURL == appURL) {
        found = true;
        var pendingUninstall = app.uninstall();
        pendingUninstall.onsuccess = function(r) {
          finished = true;
          check(true, "app has been uninstalled");
          try {
            var secondUninstall = app.uninstall();
            secondUninstall.onsuccess = function(r) {
              check(false, "mozApps allowed second uninstall without error");
              next();
            };
            secondUninstall.onerror = function(r) {
              debug("Got second error: " + this.error.name);
              check(
                this.error.name == "NOT_INSTALLED",
                "The second mozApps uninstall should return an error with the name " +
                "NOT_INSTALLED, not " + this.error.name);
              next();
            };
          } 
          catch(e) {
            check(false, "Unexpected error calling uninstall: " + e);
            next();
          }
        };
        pendingUninstall.onerror = function () {
          check(false, "Got error in uninstall: " + this.error.name);
          finished = true;
        };
      }
    }
    if (! found) {
      check(false, "Found no app with manifest URL: " + appURL);
    }
  };
  pending.onerror = function ()  {
    check(false, "Unexpected on error called in uninstall " );
  };
}

/**
 * A function that recurses a javascript object, and compares with the template to determine if the object has the expected properties
 * @check An abstraction over ok / todo to allow for that determination to be made by the invoking code
 * @object Most likely a DOM object
 */

function js_traverse(template, check, object) {
  var type = typeof template;

  // We apply SpecialPowers wrappers to the template to signal cases where
  // we need to wrap the object to examine it. We do things this way, rather
  // than wrapping unconditionally, to make sure that the default environment of
  // the test suite is as close to that of the web as possible.
  if (SpecialPowers.isWrapper(template))
    object = SpecialPowers.wrap(object);

  if (type == "object") {
    if (Object.keys(template).length == 0) {
      check(!object || object.length == 0,"The return object from mozApps api was null as expected");
      return;
    }
    for (var key in template) {
      debug("key: " + key);
      var accessor = key.split(".",1);
      if (accessor.length > 1) {
        js_traverse(template[key], check, object[accessor[0]].accessor[1]);
      } else {
        var value = (key == "status") ? SpecialPowers.wrap(object)[key] : object[key];
        if(value) {
          js_traverse(template[key], check, value);
        } else {
          check(typeof template[key] == "undefined", key + " is undefined as expected");
        }
      }
    }
  } else {
    var evaluate = "";
    
    try {
      debug("object = " + object.quote());
      debug("template = " + template);
      
      evaluate = object.quote() + template;
    } 
    catch (e) {
      debug("template = " + template);
      evaluate = object + template;
    }
    
    debug("evaluate = " + evaluate);
    debug(eval(evaluate));
    check(eval(evaluate), "#" + object + "# is expected to be true per template #" + template + "#");
  }
}

/**
 * Used to compare the result of a navigator.mozApps query, and jump to the next operation
 * @pending the variable hold the result of the navigator.mozApps query
 * @comparatorObj the array of json objects, representing what should be returned from the mozApps query 
 * @check An abstraction over ok / todo to allow for that determination to be made by the invoking code
 * @next  The next operation to jump to
 */
function mozAppscb(pending, comparatorObj, expectedStatus, check, next) {
  debug("inside mozAppscb"); 
  pending.onsuccess = function () {
    debug("success cb, called");
    check(expectedStatus == "success", "the success callback was called");
    if(pending.result) {
      if(typeof pending.result.length !== 'undefined') {
        for(i=0;i < pending.result.length;i++) {
          js_traverse(comparatorObj[i], check, pending.result[i]);
        }
      } else {
        debug("comparatorOBj in else");
        js_traverse(comparatorObj[0], check, pending.result);
      }
    } else {
      js_traverse(comparatorObj[0], check, null);
    }
    if(typeof next == 'function') {
      debug("calling next");
      next();
    }
  };

  pending.onerror = function () {
    debug("failure cb called");
    check(expectedStatus == "error", "the error callback was called");
    js_traverse(comparatorObj[0], check, pending.error);
    if(typeof next == 'function') {
      debug("calling next");
      next();
    }
  };
}

/**
 * Helper function to step through the various steps of the test
 * @steps an array of steps that we need to traverse in order to complete the test
 */

function runAll(steps) {
  var index = 0;
  SimpleTest.waitForExplicitFinish();
  function callNext() {
    if (index >= steps.length - 1) {
      steps[index]();
      SimpleTest.finish();
      return;
    }
    debug("index = " + index);
    var func = steps[index];
    index++;
    func(callNext);
  }
  callNext();
}

/**
 * Uninstall uninstalls a specific App
 * @appURL The manifest app url
 * @check An abstraction over ok / todo to allow for that determination to be made by the invoking code
 * @next  The next operation to jump to, this might need to be invoked by the iframe when the test has completed
 */

function install(appURL, check, next) {
  var origin = getOrigin(appURL);
  debug("origin = " + origin);
  var installOrigin = getOrigin(window.location.href);
  debug("installOrigin = " + installOrigin);
  var url = appURL.substring(appURL.indexOf('/apps/'));
  var manifest = JSON.parse(readFile(url));
  if(!manifest.installs_allowed_from) {
    manifest.installs_allowed_from = undefined;
  } else {
    for (var i = 0 ; i <  manifest.installs_allowed_from.length; i++)
      manifest.installs_allowed_from[i] = "== " + manifest.installs_allowed_from[i].quote();
  }
  mozAppscb(navigator.mozApps.install(
      appURL, null),
      [{
        installOrigin: "== " + installOrigin.quote(),
        installTime: "!== undefined",
        origin: "== " + origin.quote(),
        manifestURL: "== " +  appURL.quote(),
        // |manifest| is not accessible to content, so js_traverse needs to
        // use SpecialPowers to see it. We signal this by wrapping it.
        manifest: SpecialPowers.wrap({
          name: "== " + unescape(manifest.name).quote(),
          installs_allowed_from: manifest.installs_allowed_from
        })
      }], "success", check, 
      next);
}

/**
 * Gets an installed App and compares it against the expected
 * @appURL The manifest app url
 * @check An abstraction over ok / todo to allow for that determination to be made by the invoking code
 * @next  The next operation to jump to, this might need to be invoked by the iframe when the test has completed
 */

function getInstalled(appURLs, check, next) {
  var checkInstalled = [];
  for (var i = 0; i < appURLs.length ; i++) {
    var appURL = appURLs[i];
    var origin = getOrigin(appURL);
    var url = appURL.substring(appURL.indexOf('/apps/'));
    var manifest = JSON.parse(readFile(url));
   
    if(manifest.installs_allowed_from) {
      for (var j = 0 ; j <  manifest.installs_allowed_from.length; j++)
        manifest.installs_allowed_from[j] = "== " + manifest.installs_allowed_from[j].quote();
    }
    
    checkInstalled[i] = {
        installOrigin: "== " + "chrome://mochitests".quote(),
        origin: "== " + origin.quote(),
        manifestURL: "== " +  appURL.quote(),
        installTime: " \> Date.now() - 3000",
        // |manifest| is not accessible to content, so js_traverse needs to
        // use SpecialPowers to see it. We signal this by wrapping it.
        manifest: SpecialPowers.wrap({
          name: "== " + unescape(manifest.name).quote(),
          installs_allowed_from: manifest.installs_allowed_from
        })
     };
  }
  debug(JSON.stringify(checkInstalled));
  mozAppscb(navigator.mozApps.getInstalled(), checkInstalled, "success", check, next);
}

/**
 * debug function, which basically controls what goes to stdout
 * @msg  The message you want to output
 */
function debug(msg) {
  if(DEBUG == true)  {
    dump(msg + "\n");
  }
}
function check_event_listener_fired (next) {
  todo(triggered, "Event Listener fired");
  triggered = false;
  next();
}
