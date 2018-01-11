/**
 * Listen for notifications from the child.
 * These are sent in case of error, or when the loads we await have completed.
 */
window.addEventListener("message", function(event) {
    if (event.data == "childLoadComplete") {
      // all loads happen, continue the test.
      advance();
    } else if (event.data == "childOverload") {
      // too many loads happened in a test frame, abort.
      ok(false, "Too many load handlers called in test.");
      SimpleTest.finish();
    } else if (event.data.indexOf("fail-") == 0) {
      // something else failed in the test frame, abort.
      ok(false, "Child failed the test with error " + event.data.substr(5));
      SimpleTest.finish();
    }});


/**
 * helper to perform an XHR.
 */
function doXHR(url, onSuccess, onFail) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    if (xhr.status == 200) {
      onSuccess(xhr);
    } else {
      onFail(xhr);
    }
  };
  xhr.open('GET', url, true);
  xhr.send(null);
}



/**
 * This triggers state-resetting on the counter server.
 */
function resetCounter() {
  doXHR('/tests/dom/base/test/bug704320_counter.sjs?reset',
        advance,
        function(xhr) {
          ok(false, "Need to be able to reset the request counter");
          SimpleTest.finish();
        });
}

/**
 * Grabs the results via XHR and passes to checker.
 */
function checkIndividualResults(testname, expected) {
  doXHR('/tests/dom/base/test/bug704320_counter.sjs?results',
        function(xhr) {
          var results = JSON.parse(xhr.responseText);
          info(xhr.responseText);

          ok('img' in results,
              testname + " test: some image loads required in results object.");
          is(results['img'].count, 2,
            testname + " Test: Expected 2 loads for image requests.");

          expected.forEach(function (ref) {
            ok(results['img'].referrers.indexOf(ref) >= 0,
                testname + " Test: Expected " + ref + " referrer policy in test, results were " +
                JSON.stringify(results['img'].referrers) +".");
            });
          advance();
        },
        function(xhr) {
          ok(false, "Can't get results from the counter server.");
          SimpleTest.finish();
        });
}

/**
 * Grabs the results via XHR and checks them
 */
function checkExpectedGlobalResults() {
  var url = 'bug704320.sjs?action=get-test-results';
  doXHR(url,
	function(xhr) {
	      var response = JSON.parse(xhr.response);

	      for (type in response) {
		for (scheme in response[type]) {
		  for (policy in response[type][scheme]) {
		    var expectedResult = EXPECTED_RESULTS[type] === undefined ?
		      	EXPECTED_RESULTS['default'][scheme][policy] :
		      	EXPECTED_RESULTS[type][scheme][policy];
		    is(response[type][scheme][policy], expectedResult, type + ' ' + scheme + ' ' + policy);
		  }
		}
	      }
		advance();
	},
	function(xhr) {
          	ok(false, "Can't get results from the counter server.");
		SimpleTest.finish();
	});
}


var EXPECTED_RESULTS = {
  // From docshell/base/nsDocShell.cpp:
  //   "If the document containing the hyperlink being audited was not retrieved
  //    over an encrypted connection and its address does not have the same
  //    origin as "ping URL", send a referrer."
  'link-ping': {
    // Same-origin
    'http-to-http': {
      'no-referrer': '',
      'unsafe-url': '',
      'origin': '',
      'origin-when-cross-origin': '',
      'no-referrer-when-downgrade': '',
      'same-origin': '',
      'strict-origin': '',
      'strict-origin-when-cross-origin':''
    },
    'http-to-https': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=https&policy=unsafe-url',
      'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=https&policy=no-referrer-when-downgrade',
      'same-origin': '',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/'
    },
    // Encrypted and not same-origin
    'https-to-http': {
      'no-referrer': '',
      'unsafe-url': '',
      'origin': '',
      'origin-when-cross-origin': '',
      'no-referrer-when-downgrade': '',
      'same-origin': '',
      'strict-origin': '',
      'strict-origin-when-cross-origin':''
    },
    // Encrypted
    'https-to-https': {
      'no-referrer': '',
      'unsafe-url': '',
      'origin': '',
      'origin-when-cross-origin': '',
      'no-referrer-when-downgrade': '',
      'same-origin': '',
      'strict-origin': '',
      'strict-origin-when-cross-origin':''
    }
  },
  // form is tested in a 2nd level iframe.
  'form': {
    'http-to-http': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=unsafe-url&type=form',
      'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=origin-when-cross-origin&type=form',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=no-referrer-when-downgrade&type=form',
      'same-origin': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=same-origin&type=form',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=strict-origin-when-cross-origin&type=form'
    },
    'http-to-https': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=https&policy=unsafe-url&type=form',
      'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=https&policy=no-referrer-when-downgrade&type=form',
      'same-origin': '',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/'
    },
    'https-to-http': {
      'no-referrer': '',
      'unsafe-url': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=http&policy=unsafe-url&type=form',
      'origin': 'https://example.com/',
      'origin-when-cross-origin': 'https://example.com/',
      'no-referrer-when-downgrade': '',
      'same-origin': '',
      'strict-origin': '',
      'strict-origin-when-cross-origin':''
    },
    'https-to-https': {
      'no-referrer': '',
      'unsafe-url': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=unsafe-url&type=form',
      'origin': 'https://example.com/',
     'origin-when-cross-origin': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=origin-when-cross-origin&type=form',
      'no-referrer-when-downgrade': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=no-referrer-when-downgrade&type=form',
      'same-origin': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=same-origin&type=form',
      'strict-origin': 'https://example.com/',
      'strict-origin-when-cross-origin':'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=strict-origin-when-cross-origin&type=form'
    }
  },
  // window.location is tested in a 2nd level iframe.
  'window.location': {
    'http-to-http': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=unsafe-url&type=window.location',
     'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=origin-when-cross-origin&type=window.location',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=no-referrer-when-downgrade&type=window.location',
      'same-origin': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=same-origin&type=window.location',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=http&policy=strict-origin-when-cross-origin&type=window.location'
    },
    'http-to-https': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=https&policy=unsafe-url&type=window.location',
      'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=http&scheme-to=https&policy=no-referrer-when-downgrade&type=window.location',
      'same-origin': '',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/'
    },
    'https-to-http': {
      'no-referrer': '',
      'unsafe-url': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=http&policy=unsafe-url&type=window.location',
      'origin': 'https://example.com/',
      'origin-when-cross-origin': 'https://example.com/',
      'no-referrer-when-downgrade': '',
      'same-origin': '',
      'strict-origin': '',
      'strict-origin-when-cross-origin':''
    },
    'https-to-https': {
      'no-referrer': '',
      'unsafe-url': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=unsafe-url&type=window.location',
      'origin': 'https://example.com/',
      'origin-when-cross-origin': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=origin-when-cross-origin&type=window.location',
      'no-referrer-when-downgrade': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=no-referrer-when-downgrade&type=window.location',
      'same-origin': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=same-origin&type=window.location',
      'strict-origin': 'https://example.com/',
      'strict-origin-when-cross-origin':'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-2nd-level-iframe&scheme-from=https&scheme-to=https&policy=strict-origin-when-cross-origin&type=window.location'
    }
  },
  'default': {
    'http-to-http': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=http&policy=unsafe-url',
      'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=http&policy=origin-when-cross-origin',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=http&policy=no-referrer-when-downgrade',
      'same-origin': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=http&policy=same-origin',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=http&policy=strict-origin-when-cross-origin'
    },
    'http-to-https': {
      'no-referrer': '',
      'unsafe-url': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=https&policy=unsafe-url',
      'origin': 'http://example.com/',
      'origin-when-cross-origin': 'http://example.com/',
      'no-referrer-when-downgrade': 'http://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=http&scheme-to=https&policy=no-referrer-when-downgrade',
      'same-origin': '',
      'strict-origin': 'http://example.com/',
      'strict-origin-when-cross-origin':'http://example.com/'
    },
    'https-to-http': {
      'no-referrer': '',
      'unsafe-url': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=https&scheme-to=http&policy=unsafe-url',
      'origin': 'https://example.com/',
      'origin-when-cross-origin': 'https://example.com/',
      'no-referrer-when-downgrade': '',
      'same-origin': '',
      'strict-origin': '',
      'strict-origin-when-cross-origin':''
    },
    'https-to-https': {
      'no-referrer': '',
      'unsafe-url': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=https&scheme-to=https&policy=unsafe-url',
      'origin': 'https://example.com/',
      'origin-when-cross-origin': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=https&scheme-to=https&policy=origin-when-cross-origin',
      'no-referrer-when-downgrade': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=https&scheme-to=https&policy=no-referrer-when-downgrade',
      'same-origin': 'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=https&scheme-to=https&policy=same-origin',
      'strict-origin': 'https://example.com/',
      'strict-origin-when-cross-origin':'https://example.com/tests/dom/base/test/bug704320.sjs?action=create-1st-level-iframe&scheme-from=https&scheme-to=https&policy=strict-origin-when-cross-origin'
    }
  }
};
