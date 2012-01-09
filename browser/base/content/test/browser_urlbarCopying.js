/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const trimPref = "browser.urlbar.trimURLs";
const phishyUserPassPref = "network.http.phishy-userpass-length";

function test() {

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
    Services.prefs.clearUserPref(trimPref);
    Services.prefs.clearUserPref(phishyUserPassPref);
    URLBarSetURI();
  });

  Services.prefs.setBoolPref(trimPref, true);
  Services.prefs.setIntPref(phishyUserPassPref, 32); // avoid prompting about phishing

  waitForExplicitFinish();

  nextTest();
}

var tests = [
  // pageproxystate="invalid"
  {
    setURL: "http://example.com/",
    expectedURL: "example.com",
    copyExpected: "example.com"
  },
  {
    copyVal: "<e>xample.com",
    copyExpected: "e"
  },

  // pageproxystate="valid" from this point on (due to the load)
  {
    loadURL: "http://example.com/",
    expectedURL: "example.com",
    copyExpected: "http://example.com/"
  },
  {
    copyVal: "<example.co>m",
    copyExpected: "example.co"
  },
  {
    copyVal: "e<x>ample.com",
    copyExpected: "x"
  },
  {
    copyVal: "<e>xample.com",
    copyExpected: "e"
  },

  {
    loadURL: "http://example.com/foo",
    expectedURL: "example.com/foo",
    copyExpected: "http://example.com/foo"
  },
  {
    copyVal: "<example.com>/foo",
    copyExpected: "http://example.com"
  },
  {
    copyVal: "<example>.com/foo",
    copyExpected: "example"
  },

  // Test that userPass is stripped out
  {
    loadURL: "http://user:pass@mochi.test:8888/browser/browser/base/content/test/authenticate.sjs?user=user&pass=pass",
    expectedURL: "mochi.test:8888/browser/browser/base/content/test/authenticate.sjs?user=user&pass=pass",
    copyExpected: "http://mochi.test:8888/browser/browser/base/content/test/authenticate.sjs?user=user&pass=pass"
  },

  // Test escaping
  {
    loadURL: "http://example.com/()%C3%A9",
    expectedURL: "example.com/()\xe9",
    copyExpected: "http://example.com/%28%29%C3%A9"
  },
  {
    copyVal: "<example.com/(>)\xe9",
    copyExpected: "http://example.com/("
  },
  {
    copyVal: "e<xample.com/(>)\xe9",
    copyExpected: "xample.com/("
  },

  {
    loadURL: "http://example.com/%C3%A9%C3%A9",
    expectedURL: "example.com/\xe9\xe9",
    copyExpected: "http://example.com/%C3%A9%C3%A9"
  },
  {
    copyVal: "e<xample.com/\xe9>\xe9",
    copyExpected: "xample.com/\xe9"
  },
  {
    copyVal: "<example.com/\xe9>\xe9",
    copyExpected: "http://example.com/\xe9"
  },

  {
    loadURL: "http://example.com/?%C3%B7%C3%B7",
    expectedURL: "example.com/?\xf7\xf7",
    copyExpected: "http://example.com/?%C3%B7%C3%B7"
  },
  {
    copyVal: "e<xample.com/?\xf7>\xf7",
    copyExpected: "xample.com/?\xf7"
  },
  {
    copyVal: "<example.com/?\xf7>\xf7",
    copyExpected: "http://example.com/?\xf7"
  },

  // data: and javsacript: URIs shouldn't be encoded
  {
    loadURL: "javascript:('%C3%A9')",
    expectedURL: "javascript:('\xe9')",
    copyExpected: "javascript:('\xe9')"
  },
  {
    copyVal: "<javascript:(>'\xe9')",
    copyExpected: "javascript:("
  },

  {
    loadURL: "data:text/html,(%C3%A9)",
    expectedURL: "data:text/html,(\xe9)",
    copyExpected: "data:text/html,(\xe9)"
  },
  {
    copyVal: "<data:text/html,(>\xe9)",
    copyExpected: "data:text/html,("
  },
  {
    copyVal: "data:<text/html,(\xe9>)",
    copyExpected: "text/html,(\xe9"
  }
];

function nextTest() {
  let test = tests.shift();
  if (tests.length == 0)
    runTest(test, finish);
  else
    runTest(test, nextTest);
}

function runTest(test, cb) {
  function doCheck() {
    if (test.setURL || test.loadURL) {
      gURLBar.valueIsTyped = !!test.setURL;
      is(gURLBar.value, test.expectedURL, "url bar value set");
    }

    testCopy(test.copyVal, test.copyExpected, cb);
  }

  if (test.loadURL) {
    loadURL(test.loadURL, doCheck);
  } else {
    if (test.setURL)
      gURLBar.value = test.setURL;
    doCheck();
  }
}

function testCopy(copyVal, targetValue, cb) {
  info("Expecting copy of: " + targetValue);
  waitForClipboard(targetValue, function () {
    gURLBar.focus();
    if (copyVal) {
      let startBracket = copyVal.indexOf("<");
      let endBracket = copyVal.indexOf(">");
      if (startBracket == -1 || endBracket == -1 ||
          startBracket > endBracket ||
          copyVal.replace("<", "").replace(">", "") != gURLBar.value) {
        ok(false, "invalid copyVal: " + copyVal);
      }
      gURLBar.selectionStart = startBracket;
      gURLBar.selectionEnd = endBracket - 1;
    } else {
      gURLBar.select();
    }

    goDoCommand("cmd_copy");
  }, cb, cb);
}

function loadURL(aURL, aCB) {
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    is(gBrowser.currentURI.spec, aURL, "loaded expected URL");
    aCB();
  }, true);

  gBrowser.loadURI(aURL);
}
