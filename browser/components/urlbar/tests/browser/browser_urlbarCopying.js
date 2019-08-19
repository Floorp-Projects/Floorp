/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const trimPref = "browser.urlbar.trimURLs";
const phishyUserPassPref = "network.http.phishy-userpass-length";
const decodeURLpref = "browser.urlbar.decodeURLsOnCopy";

function getUrl(hostname, file) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      hostname
    ) + file
  );
}

function test() {
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));

  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
    Services.prefs.clearUserPref(trimPref);
    Services.prefs.clearUserPref(phishyUserPassPref);
    Services.prefs.clearUserPref(decodeURLpref);
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
    copyExpected: "example.com",
  },
  {
    copyVal: "<e>xample.com",
    copyExpected: "e",
  },
  {
    copyVal: "<e>x<a>mple.com",
    copyExpected: "ea",
  },
  {
    copyVal: "<e><xa>mple.com",
    copyExpected: "exa",
  },
  {
    copyVal: "<e><xa>mple.co<m>",
    copyExpected: "exam",
  },
  {
    copyVal: "<e><xample.co><m>",
    copyExpected: "example.com",
  },

  // pageproxystate="valid" from this point on (due to the load)
  {
    loadURL: "http://example.com/",
    expectedURL: "example.com",
    copyExpected: "http://example.com/",
  },
  {
    copyVal: "<example.co>m",
    copyExpected: "example.co",
  },
  {
    copyVal: "e<x>ample.com",
    copyExpected: "x",
  },
  {
    copyVal: "<e>xample.com",
    copyExpected: "e",
  },
  {
    copyVal: "<e>xample.co<m>",
    copyExpected: "em",
  },
  {
    copyVal: "<exam><ple.com>",
    copyExpected: "example.com",
  },

  {
    loadURL: "http://example.com/foo",
    expectedURL: "example.com/foo",
    copyExpected: "http://example.com/foo",
  },
  {
    copyVal: "<example.com>/foo",
    copyExpected: "http://example.com",
  },
  {
    copyVal: "<example>.com/foo",
    copyExpected: "example",
  },

  // Test that userPass is stripped out
  {
    loadURL: getUrl(
      "http://user:pass@mochi.test:8888",
      "authenticate.sjs?user=user&pass=pass"
    ),
    expectedURL: getUrl(
      "mochi.test:8888",
      "authenticate.sjs?user=user&pass=pass"
    ),
    copyExpected: getUrl(
      "http://mochi.test:8888",
      "authenticate.sjs?user=user&pass=pass"
    ),
  },

  // Test escaping
  {
    loadURL: "http://example.com/()%28%29%C3%A9",
    expectedURL: "example.com/()()\xe9",
    copyExpected: "http://example.com/()%28%29%C3%A9",
  },
  {
    copyVal: "<example.com/(>)()\xe9",
    copyExpected: "http://example.com/(",
  },
  {
    copyVal: "e<xample.com/(>)()\xe9",
    copyExpected: "xample.com/(",
  },

  {
    loadURL: "http://example.com/%C3%A9%C3%A9",
    expectedURL: "example.com/\xe9\xe9",
    copyExpected: "http://example.com/%C3%A9%C3%A9",
  },
  {
    copyVal: "e<xample.com/\xe9>\xe9",
    copyExpected: "xample.com/\xe9",
  },
  {
    copyVal: "<example.com/\xe9>\xe9",
    copyExpected: "http://example.com/\xe9",
  },
  {
    // Note: it seems BrowserTestUtils.loadURI fails for unicode domains
    loadURL: "http://sub2.xn--lt-uia.mochi.test:8888/foo",
    expectedURL: "sub2.ält.mochi.test:8888/foo",
    copyExpected: "http://sub2.ält.mochi.test:8888/foo",
  },
  {
    copyVal: "s<ub2.ält.mochi.test:8888/f>oo",
    copyExpected: "ub2.ält.mochi.test:8888/f",
  },
  {
    copyVal: "<sub2.ält.mochi.test:8888/f>oo",
    copyExpected: "http://sub2.ält.mochi.test:8888/f",
  },

  {
    loadURL: "http://example.com/?%C3%B7%C3%B7",
    expectedURL: "example.com/?\xf7\xf7",
    copyExpected: "http://example.com/?%C3%B7%C3%B7",
  },
  {
    copyVal: "e<xample.com/?\xf7>\xf7",
    copyExpected: "xample.com/?\xf7",
  },
  {
    copyVal: "<example.com/?\xf7>\xf7",
    copyExpected: "http://example.com/?\xf7",
  },
  {
    loadURL: "http://example.com/a%20test",
    expectedURL: "example.com/a test",
    copyExpected: "http://example.com/a%20test",
  },
  {
    loadURL: "http://example.com/a%E3%80%80test",
    expectedURL: "example.com/a\u{3000}test",
    copyExpected: "http://example.com/a%E3%80%80test",
  },
  {
    loadURL: "http://example.com/a%20%C2%A0test",
    expectedURL: "example.com/a%20%C2%A0test",
    copyExpected: "http://example.com/a%20%C2%A0test",
  },
  {
    loadURL: "http://example.com/%20%20%20",
    expectedURL: "example.com/%20%20%20",
    copyExpected: "http://example.com/%20%20%20",
  },
  {
    loadURL: "http://example.com/%E3%80%80%E3%80%80",
    expectedURL: "example.com/%E3%80%80%E3%80%80",
    copyExpected: "http://example.com/%E3%80%80%E3%80%80",
  },

  // data: and javsacript: URIs shouldn't be encoded
  {
    loadURL: "javascript:('%C3%A9%20%25%50')",
    expectedURL: "javascript:('%C3%A9 %25P')",
    copyExpected: "javascript:('%C3%A9 %25P')",
  },
  {
    copyVal: "<javascript:(>'%C3%A9 %25P')",
    copyExpected: "javascript:(",
  },

  {
    loadURL: "data:text/html,(%C3%A9%20%25%50)",
    expectedURL: "data:text/html,(%C3%A9 %25P)",
    copyExpected: "data:text/html,(%C3%A9 %25P)",
  },
  {
    copyVal: "<data:text/html,(>%C3%A9 %25P)",
    copyExpected: "data:text/html,(",
  },
  {
    copyVal: "<data:text/html,(%C3%A9 %25P>)",
    copyExpected: "data:text/html,(%C3%A9 %25P",
  },
  {
    setup() {
      Services.prefs.setBoolPref(decodeURLpref, true);
    },
    loadURL:
      "http://example.com/%D0%B1%D0%B8%D0%BE%D0%B3%D1%80%D0%B0%D1%84%D0%B8%D1%8F",
    expectedURL: "example.com/биография",
    copyExpected: "http://example.com/биография",
  },
  {
    copyVal: "<example.com/би>ография",
    copyExpected: "http://example.com/би",
  },
];

function nextTest() {
  let testCase = tests.shift();
  if (tests.length == 0) {
    runTest(testCase, finish);
  } else {
    runTest(testCase, nextTest);
  }
}

function runTest(testCase, cb) {
  function doCheck() {
    if (testCase.setURL || testCase.loadURL) {
      gURLBar.valueIsTyped = !!testCase.setURL;
      is(gURLBar.value, testCase.expectedURL, "url bar value set");
    }

    testCopy(testCase.copyVal, testCase.copyExpected, cb);
  }

  if (testCase.setup) {
    testCase.setup();
  }

  if (testCase.loadURL) {
    info(`Loading : ${testCase.loadURL}\n`);
    loadURL(testCase.loadURL, doCheck);
  } else {
    if (testCase.setURL) {
      gURLBar.value = testCase.setURL;
    }
    doCheck();
  }
}

function testCopy(copyVal, targetValue, cb) {
  info("Expecting copy of: " + targetValue);

  gURLBar.focus();
  if (copyVal) {
    let offsets = [];
    while (true) {
      let startBracket = copyVal.indexOf("<");
      let endBracket = copyVal.indexOf(">");
      if (startBracket == -1 && endBracket == -1) {
        break;
      }
      if (startBracket > endBracket || startBracket == -1) {
        offsets = [];
        break;
      }
      offsets.push([startBracket, endBracket - 1]);
      copyVal = copyVal.replace("<", "").replace(">", "");
    }
    if (offsets.length == 0 || copyVal != gURLBar.value) {
      ok(false, "invalid copyVal: " + copyVal);
    }
    gURLBar.selectionStart = offsets[0][0];
    gURLBar.selectionEnd = offsets[0][1];
    if (offsets.length > 1) {
      let sel = gURLBar.editor.selection;
      let r0 = sel.getRangeAt(0);
      let node0 = r0.startContainer;
      sel.removeAllRanges();
      offsets.map(function(startEnd) {
        let range = r0.cloneRange();
        range.setStart(node0, startEnd[0]);
        range.setEnd(node0, startEnd[1]);
        sel.addRange(range);
      });
    }
  } else {
    gURLBar.select();
  }

  waitForClipboard(
    targetValue,
    function() {
      goDoCommand("cmd_copy");
    },
    cb,
    cb
  );
}

function loadURL(aURL, aCB) {
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, aURL);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, aURL).then(
    aCB
  );
}
