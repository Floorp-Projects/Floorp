/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getUrl(hostname, file) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      hostname
    ) + file
  );
}

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
    gURLBar.setURI();
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.trimURLs", true],
      // avoid prompting about phishing
      ["network.http.phishy-userpass-length", 32],
    ],
  });

  for (let testCase of tests) {
    if (testCase.setup) {
      await testCase.setup();
    }

    if (testCase.loadURL) {
      info(`Loading : ${testCase.loadURL}`);
      let expectedLoad = testCase.expectedLoad || testCase.loadURL;
      BrowserTestUtils.loadURI(gBrowser.selectedBrowser, testCase.loadURL);
      await BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        expectedLoad
      );
    } else if (testCase.setURL) {
      gURLBar.value = testCase.setURL;
    }
    if (testCase.setURL || testCase.loadURL) {
      gURLBar.valueIsTyped = !!testCase.setURL;
      is(
        gURLBar.value,
        testCase.expectedURL,
        "url bar value set to " + gURLBar.value
      );
    }

    gURLBar.focus();
    if (testCase.expectedValueOnFocus) {
      Assert.equal(
        gURLBar.value,
        testCase.expectedValueOnFocus,
        "Check value on focus"
      );
    }
    await testCopy(testCase.copyVal, testCase.copyExpected);
    gURLBar.blur();

    if (testCase.cleanup) {
      await testCase.cleanup();
    }
  }
});

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
    expectedURL: "example.com/a%E3%80%80test",
    copyExpected: "http://example.com/a%E3%80%80test",
  },
  {
    loadURL: "http://example.com/a%20%C2%A0test",
    expectedURL: "example.com/a %C2%A0test",
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

  // Loading of javascript: URI results in previous URI, so if the previous
  // entry changes, change this one too!
  {
    loadURL: "javascript:('%C3%A9%20%25%50')",
    expectedLoad: "http://example.com/%E3%80%80%E3%80%80",
    expectedURL: "example.com/%E3%80%80%E3%80%80",
    copyExpected: "http://example.com/%E3%80%80%E3%80%80",
  },

  // data: URIs shouldn't be encoded
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
    async setup() {
      await SpecialPowers.pushPrefEnv({
        set: [["browser.urlbar.decodeURLsOnCopy", true]],
      });
    },
    async cleanup() {
      await SpecialPowers.popPrefEnv();
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

  {
    async setup() {
      await SpecialPowers.pushPrefEnv({
        set: [["browser.urlbar.decodeURLsOnCopy", true]],
      });
      // Setup a valid intranet url that resolves but is not yet known.
      const proxyService = Cc[
        "@mozilla.org/network/protocol-proxy-service;1"
      ].getService(Ci.nsIProtocolProxyService);
      let proxyInfo = proxyService.newProxyInfo(
        "http",
        "localhost",
        8888,
        "",
        "",
        0,
        4096,
        null
      );
      this._proxyFilter = {
        applyFilter(channel, defaultProxyInfo, callback) {
          callback.onProxyFilterResult(
            channel.URI.host === "mytest" ? proxyInfo : defaultProxyInfo
          );
        },
      };
      proxyService.registerChannelFilter(this._proxyFilter, 0);
      registerCleanupFunction(() => {
        if (this._proxyFilter) {
          proxyService.unregisterChannelFilter(this._proxyFilter);
        }
      });
    },
    async cleanup() {
      await SpecialPowers.popPrefEnv();
      const proxyService = Cc[
        "@mozilla.org/network/protocol-proxy-service;1"
      ].getService(Ci.nsIProtocolProxyService);
      proxyService.unregisterChannelFilter(this._proxyFilter);
      this._proxyFilter = null;
    },
    loadURL: "http://mytest/",
    expectedURL: "mytest",
    expectedValueOnFocus: "http://mytest/",
    copyExpected: "http://mytest/",
  },

  {
    async setup() {
      await SpecialPowers.pushPrefEnv({
        set: [["browser.urlbar.decodeURLsOnCopy", true]],
      });
    },
    async cleanup() {
      await SpecialPowers.popPrefEnv();
    },
    loadURL: "https://example.com/",
    expectedURL: "https://example.com",
    copyExpected: "https://example.com",
  },
];

function testCopy(copyVal, targetValue) {
  info("Expecting copy of: " + targetValue);

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
    if (!offsets.length || copyVal != gURLBar.value) {
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

  return SimpleTest.promiseClipboardChange(targetValue, () =>
    goDoCommand("cmd_copy")
  );
}
