/** Test for Bug 815105 **/
/*
 * gData is an array of object that tests using this framework must pass in
 * The current tests only pass in a single element array. Each test in
 * gData is executed by the framework for a given file
 *
 * Fields in gData object
 * perms   (required) Array of Strings
 *         list of permissions that this test will need. See
 *         http://mxr.mozilla.org/mozilla-central/source/dom/apps/src/PermissionsTable.jsm
 *         These permissions are added after a sanity check and removed at
 *         test conclusion
 *
 * obj     (required for default verifier) String
 *         The name of the window.navigator object used for accessing the
 *         WebAPI during the tests
 *
 * webidl  (required for default verifier) String
 * idl     (required for default verifier) String
 *         Only one of webidl / idl is required
 *         The IDL describing the navigator object. The returned object
 *         during tests /must/ be an instanceof this
 *
 * skip    (optional) Array of Strings
 *         A list of navigator.userAgent's to skip the second part of tests
 *         on. The tests still verify that you can't get obj on those
 *         platforms without permissions, however it is expected that adding
 *         the permission still won't allow access to those objects
 *
 * settings (optional) Array of preference tuples
 *         A list of settings that need to be set before this API is
 *         enabled. Note the settings are set before the sanity check is
 *         performed. If an API gates access only by preferences, then it
 *         will fail the initial test
 *
 * verifier (optional) Function
 *         A function used to test whether a WebAPI is accessible or not.
 *         The function takes a success and failure callback which both
 *         accept a msg argument. msg is surfaced up to the top level tests
 *         A default verifier is provided which only attempts to access
 *         the navigator object.
 *
 * needParentPerm (optional) Boolean
 *         Whether or not the parent frame requires these permissions as
 *         well. Otherwise the test process may be killed.
 */

SimpleTest.waitForExplicitFinish();
var expand = SpecialPowers.Cu.import("resource://gre/modules/PermissionsTable.jsm").expandPermissions;
const permTable = SpecialPowers.Cu.import("resource://gre/modules/PermissionsTable.jsm").PermissionsTable;

const TEST_DOMAIN = "http://example.org";
const SHIM_PATH = "/tests/dom/permission/tests/file_shim.html"
var gContent = document.getElementById('content');

//var gData; defined in external files
var gCurrentTest = 0;
var gRemainingTests;
var pendingTests = {};

function PermTest(aData) {
  var self = this;
  var skip = aData.skip || false;
  this.step = 0;
  this.data = aData;
  this.isSkip = skip &&
                skip.some(function (el) {
                          return navigator.
                                 userAgent.toLowerCase().
                                 indexOf(el.toLowerCase()) != -1;
                        });

  this.setupParent = false;
  this.perms = expandPermissions(aData.perm);
  this.id = gCurrentTest++;
  this.iframe = null;

  // keep a reference to this for eventhandler
  pendingTests[this.id] = this;

  this.createFrame = function() {
    if (self.iframe) {
      gContent.removeChild(self.iframe);
    }
    var iframe = document.createElement('iframe');
    iframe.setAttribute('id', 'testframe' + self.step + self.perms)
    iframe.setAttribute('remote', true);
    iframe.src = TEST_DOMAIN + SHIM_PATH;
    iframe.addEventListener('load', function _iframeLoad() {
      iframe.removeEventListener('load', _iframeLoad);

      // check permissions are correct
      var allow = (self.step == 0 ? false : true);
      self.perms.forEach(function (el) {
        try {
        var res = SpecialPowers.hasPermission(el, SpecialPowers.wrap(iframe)
                                                  .contentDocument);
        is(res, allow, (allow ? "Has " : "Doesn't have ") + el);
        } catch(e) {
          ok(false, "failed " + e);
        }
      });

      var msg = {
        id: self.id,
        step: self.step++,
        testdata: self.data,
      }
      // start the tests
      iframe.contentWindow.postMessage(msg, "*");
    });

    self.iframe = iframe;
    gContent.appendChild(iframe);
  }

  this.next = function () {
    switch(self.step) {
    case 0:
      self.createFrame();
    break;
    case 1:
      // add permissions
      addPermissions(self.perms, SpecialPowers.
                                 wrap(self.iframe).
                                 contentDocument,
                     self.createFrame.bind(self));
    break;
    case 2:
      if (self.iframe) {
        gContent.removeChild(self.iframe);
      }
      checkFinish();
    break;
    default:
      ok(false, "Should not be reached");
    break
    }
  }

  this.start = function() {
    // some permissions need parent to have permission as well
    if (!self.setupParent && self.data.needParentPerm &&
        !SpecialPowers.isMainProcess()) {
      self.setupParent = true;
      addPermissions(self.perms, window.document, self.start.bind(self));
    } else if (self.data.settings && self.data.settings.length) {
      SpecialPowers.pushPrefEnv({'set': self.data.settings.slice(0)},
                                self.next.bind(self));
    } else {
      self.next();
    }
  }
}

function addPermissions(aPerms, aDoc, aCallback) {
  var permList = [];
  aPerms.forEach(function (el) {
    var obj = {'type': el,
               'allow': 1,
               'context': aDoc};
    permList.push(obj);
  });
  SpecialPowers.pushPermissions(permList, aCallback);
}

function expandPermissions(aPerms) {
  var perms = [];
  aPerms.forEach(function(el) {
    var access = permTable[el].access ? "readwrite" : null;
    var expanded = expand(el, access);
    for (let i = 0; i < expanded.length; i++) {
      perms.push(SpecialPowers.unwrap(expanded[i]));
    }
  });

  return perms;
}

function msgHandler(evt) {
  var data = evt.data;
  var test = pendingTests[data.id];

  /*
   * step 2 of tests should fail on
   * platforms which are skipped
   */
  if (test.isSkip && test.step == 2) {
    todo(data.result, data.msg);
  } else {
    ok(data.result, data.msg);
  }

  if (test) {
    test.next();
  } else {
    ok(false, "Received unknown id " + data.id);
    checkFinish();
  }
}

function checkFinish() {
  if (--gRemainingTests) {
    gTestRunner.next();
  } else {
    window.removeEventListener('message', msgHandler);
    SimpleTest.finish();
  }
}

function runTest() {
  gRemainingTests = Object.keys(gData).length;

  for (var test in gData) {
    var test = new PermTest(gData[test]);
    test.start();
    yield undefined;
  }
}

var gTestRunner = runTest();

window.addEventListener('load', function() { gTestRunner.next(); }, false);
window.addEventListener('message', msgHandler, false);
