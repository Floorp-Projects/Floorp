// This helper expects these globals to be defined.
/* global PARAMS, SJS, testCases */

/*
 * common functionality for iframe, anchor, and area referrer attribute tests
 */
const GET_RESULT = SJS + 'ACTION=get-test-results';
const RESET_STATE = SJS + 'ACTION=resetState';

SimpleTest.waitForExplicitFinish();
var advance = function() { tests.next(); };

/**
 * Listen for notifications from the child.
 * These are sent in case of error, or when the loads we await have completed.
 */
window.addEventListener("message", function(event) {
  if (event.data == "childLoadComplete") {
    // all loads happen, continue the test.
    advance();
  }
});

/**
 * helper to perform an XHR
 * to do checkIndividualResults and resetState
 */
function doXHR(aUrl, onSuccess, onFail) {
  // The server is at http[s]://example.com so we need cross-origin XHR.
  var xhr = new XMLHttpRequest({mozSystem: true});
  xhr.responseType = "json";
  xhr.onload = function () {
    onSuccess(xhr);
  };
  xhr.onerror = function () {
    onFail(xhr);
  };
  xhr.open('GET', "http" + aUrl, true);
  xhr.send(null);
}

/**
 * Grabs the results via XHR and passes to checker.
 */
function checkIndividualResults(aTestname, aExpectedReferrer, aName) {
  var onload = xhr => {
    var results = xhr.response;
    info(JSON.stringify(xhr.response));
    ok(aName in results, aName + " tests have to be performed.");
    is(results[aName].policy, aExpectedReferrer, aTestname + ' --- ' + results[aName].policy + ' (' + results[aName].referrer + ')');
    advance();
  };
  var onerror = xhr => {
    ok(false, "Can't get results from the counter server.");
    SimpleTest.finish();
  };
  doXHR(GET_RESULT, onload, onerror);
}

function resetState() {
  doXHR(RESET_STATE,
    advance,
    function(xhr) {
      ok(false, "error in reset state");
      SimpleTest.finish();
    });
}

/**
 * testing if referrer header is sent correctly
 */
var tests = (function*() {

  yield SpecialPowers.pushPrefEnv({"set": [['network.preload', true]]}, advance);
  yield SpecialPowers.pushPrefEnv({"set": [['security.mixed_content.block_active_content', false]]}, advance);
  yield SpecialPowers.pushPermissions([{'type': 'systemXHR', 'allow': true, 'context': document}], advance);

  var iframe = document.getElementById("testframe");

  for (var j = 0; j < testCases.length; j++) {
    if (testCases[j].PREFS) {
      yield SpecialPowers.pushPrefEnv({"set": testCases[j].PREFS}, advance);
    }

    var actions = testCases[j].ACTION;
    var subTests = testCases[j].TESTS;
    for (var k = 0; k < actions.length; k++) {
      var actionString = actions[k];
      for (var i = 0; i < subTests.length; i++) {
        yield resetState();
        var searchParams = new URLSearchParams();
        searchParams.append("ACTION", actionString);
        searchParams.append("NAME", subTests[i].NAME);
        for (var l of PARAMS) {
          if (subTests[i][l]) {
            searchParams.append(l, subTests[i][l]);
          }
        }
        var schemeFrom = subTests[i].SCHEME_FROM || "http";
        yield iframe.src = schemeFrom + SJS + searchParams.toString();
        yield checkIndividualResults(subTests[i].DESC, subTests[i].RESULT, subTests[i].NAME);
      };
    };
  };

  // complete.
  SimpleTest.finish();
})();
