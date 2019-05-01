"use strict";

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


/*
  This test file exists to ensure whenever changes to principal serialization happens,
  we guarantee that the data can be restored and generated into a new principal.

  The tests are written to be brittle so we encode all versions of the changes into the tests.
*/

add_task(function test_nullPrincipal() {
  /*
   As Null principals are designed to be non deterministic we just need to ensure that
   a previous serialized version matches what it was generated as.

   This test should be resilient to changes in versioning, however it should also be duplicated for a new serialization change.
  */
  // Principal created with: E10SUtils.serializePrincipal(Services.scriptSecurityManager.createNullPrincipal({ }));
  let p = E10SUtils.deserializePrincipal("vQZuXxRvRHKDMXv9BbHtkAAAAAAAAAAAwAAAAAAAAEYAAAA4bW96LW51bGxwcmluY2lwYWw6ezU2Y2FjNTQwLTg2NGQtNDdlNy04ZTI1LTE2MTRlYWI1MTU1ZX0AAAAA");
  is("moz-nullprincipal:{56cac540-864d-47e7-8e25-1614eab5155e}", p.URI.spec, "Deserialized principal doesn't have the correct URI");

  // Principal created with: E10SUtils.serializePrincipal(Services.scriptSecurityManager.createNullPrincipal({ userContextId: 2 }));
  let p2 = E10SUtils.deserializePrincipal("vQZuXxRvRHKDMXv9BbHtkAAAAAAAAAAAwAAAAAAAAEYAAAA4bW96LW51bGxwcmluY2lwYWw6ezA1ZjllN2JhLWIwODMtNDJhMi1iNDdkLTZiODRmNmYwYTM3OX0AAAAQXnVzZXJDb250ZXh0SWQ9Mg==");
  is("moz-nullprincipal:{05f9e7ba-b083-42a2-b47d-6b84f6f0a379}", p2.URI.spec, "Deserialized principal doesn't have the correct URI");
  is(p2.originAttributes.userContextId, 2, "Expected a userContextId of 2");
});

add_task(async function test_contentPrincipal() {
  /*
   This test should NOT be resilient to changes in versioning,
   however it exists purely to verify the code doesn't unintentionally change without updating versioning and migration code.
  */
  let tests = [
    {
      input: ["http://example.com/", {}],
      expected: "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAFABAAAAE2h0dHA6Ly9leGFtcGxlLmNvbS8AAAAAAAAABAAAAAcAAAALAAAAB/////8AAAAH/////wAAAAcAAAALAAAAEgAAAAEAAAASAAAAAQAAABIAAAABAAAAEwAAAAAAAAAT/////wAAAAD/////AAAAEv////8AAAAS/////wEAAAAAAAAAAAAAAAA=",
    },
    {
      input: ["http://mozilla1.com/", {}],
      expected: "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAFABAAAAFGh0dHA6Ly9tb3ppbGxhMS5jb20vAAAAAAAAAAQAAAAHAAAADAAAAAf/////AAAAB/////8AAAAHAAAADAAAABMAAAABAAAAEwAAAAEAAAATAAAAAQAAABQAAAAAAAAAFP////8AAAAA/////wAAABP/////AAAAE/////8BAAAAAAAAAAAAAAAA",
    },
    {
      input: ["http://mozilla2.com/", { userContextId: 0 }],
      expected: "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAFABAAAAFGh0dHA6Ly9tb3ppbGxhMi5jb20vAAAAAAAAAAQAAAAHAAAADAAAAAf/////AAAAB/////8AAAAHAAAADAAAABMAAAABAAAAEwAAAAEAAAATAAAAAQAAABQAAAAAAAAAFP////8AAAAA/////wAAABP/////AAAAE/////8BAAAAAAAAAAAAAAAA",
    },
    {
      input: ["http://mozilla3.com/", { userContextId: 2 }],
      expected: "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAFABAAAAFGh0dHA6Ly9tb3ppbGxhMy5jb20vAAAAAAAAAAQAAAAHAAAADAAAAAf/////AAAAB/////8AAAAHAAAADAAAABMAAAABAAAAEwAAAAEAAAATAAAAAQAAABQAAAAAAAAAFP////8AAAAA/////wAAABP/////AAAAE/////8BAAAAAAAAAAAAABBedXNlckNvbnRleHRJZD0yAA==",
    },
    {
      input: ["http://mozilla4.com/", { privateBrowsingId: 1 }],
      expected: "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAFABAAAAFGh0dHA6Ly9tb3ppbGxhNC5jb20vAAAAAAAAAAQAAAAHAAAADAAAAAf/////AAAAB/////8AAAAHAAAADAAAABMAAAABAAAAEwAAAAEAAAATAAAAAQAAABQAAAAAAAAAFP////8AAAAA/////wAAABP/////AAAAE/////8BAAAAAAAAAAAAABRecHJpdmF0ZUJyb3dzaW5nSWQ9MQA=",
    },
    {
      input: ["http://mozilla5.com/", { privateBrowsingId: 0 }],
      expected: "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAFABAAAAFGh0dHA6Ly9tb3ppbGxhNS5jb20vAAAAAAAAAAQAAAAHAAAADAAAAAf/////AAAAB/////8AAAAHAAAADAAAABMAAAABAAAAEwAAAAEAAAATAAAAAQAAABQAAAAAAAAAFP////8AAAAA/////wAAABP/////AAAAE/////8BAAAAAAAAAAAAAAAA",
    },
  ];

  for (let test of tests) {
    let uri = Services.io.newURI(test.input[0]);
    let p = Services.scriptSecurityManager.createCodebasePrincipal(uri, test.input[1]);
    let sp = E10SUtils.serializePrincipal(p);
    is(test.expected, sp, "Expected serialized string for " + test.input[0]);
    let dp = E10SUtils.deserializePrincipal(sp);
    is(dp.URI.spec, test.input[0], "Ensure spec is the same");

    // Check all the origin attributes
    for (let key in test.input[1]) {
      is(dp.originAttributes[key], test.input[1][key], "Ensure value of " + key + " is " + test.input[1][key]);
    }
  }
});


add_task(async function test_realHistoryCheck() {
  /*
  This test should be resilient to changes in principal serialization, if these are failing then it's likely the code will break session storage.
  To recreate this for another version, copy the function into the browser console, browse some pages and printHistory.

  Generated with:
  function printHistory() {
    let tests = [];
    let entries = SessionStore.getSessionHistory(gBrowser.selectedTab).entries.map((entry) => { return entry.triggeringPrincipal_base64 });
    entries.push(E10SUtils.serializePrincipal(gBrowser.selectedTab.linkedBrowser._contentPrincipal));
    for (let entry of entries) {
      console.log(entry);
      let testData = {};
      testData.input = entry;
      let principal = E10SUtils.deserializePrincipal(testData.input);
      testData.output = {};
      if (principal.URI === null) {
        testData.output.URI = false;
      } else {
        testData.output.URISpec = principal.URI.spec;
      }
      testData.output.originAttributes = principal.originAttributes;
      testData.output.cspJSON = principal.cspJSON;

      tests.push(testData);
    }
    return tests;
  }
  printHistory(); // Copy this into: serializedPrincipalsFromFirefox
  */

  let serializedPrincipalsFromFirefox = [
    {
      "input": "SmIS26zLEdO3ZQBgsLbOywAAAAAAAAAAwAAAAAAAAEY=",
      "output": {
        "URI": false,
        "originAttributes": {
          "firstPartyDomain": "",
          "inIsolatedMozBrowser": false,
          "privateBrowsingId": 0,
          "userContextId": 0,
        },
        "cspJSON": "{}",
      },
    },
    {
      "input": "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAbsBAAAAHmh0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2VuLVVTLwAAAAAAAAAFAAAACAAAAA8AAAAA/////wAAAAD/////AAAACAAAAA8AAAAXAAAABwAAABcAAAAHAAAAFwAAAAcAAAAeAAAAAAAAAAD/////AAAAAP////8AAAAA/////wAAAAD/////AQAAAAAAAAAAAAAAAQnZ7Rrl1EAEv+Anzrkj2ayzxMCuvV5MrYfgjSENuz+fAd6UctCANBHTk5kAEEug/UCSBzpUbXhPMJE6uHGBMgjGAAAAAv////8AAAG7AQAAAB5odHRwczovL3d3dy5tb3ppbGxhLm9yZy9lbi1VUy8AAAAAAAAABQAAAAgAAAAPAAAAAP////8AAAAA/////wAAAAgAAAAPAAAAFwAAAAcAAAAXAAAABwAAABcAAAAHAAAAHgAAAAAAAAAA/////wAAAAD/////AAAAAP////8AAAAA/////wEAAAAAAAAAAAABAAAFtgBzAGMAcgBpAHAAdAAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIAAnAHUAbgBzAGEAZgBlAC0AaQBuAGwAaQBuAGUAJwAgACcAdQBuAHMAYQBmAGUALQBlAHYAYQBsACcAIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQB0AGEAZwBtAGEAbgBhAGcAZQByAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQAtAGEAbgBhAGwAeQB0AGkAYwBzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdABhAGcAbQBhAG4AYQBnAGUAcgAuAGcAbwBvAGcAbABlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgB5AG8AdQB0AHUAYgBlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AcwAuAHkAdABpAG0AZwAuAGMAbwBtADsAIABpAG0AZwAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIABkAGEAdABhADoAIABoAHQAdABwAHMAOgAvAC8AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUAdABhAGcAbQBhAG4AYQBnAGUAcgAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALQBhAG4AYQBsAHkAdABpAGMAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAZABzAGUAcgB2AGkAYwBlAC4AZwBvAG8AZwBsAGUALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGQAcwBlAHIAdgBpAGMAZQAuAGcAbwBvAGcAbABlAC4AZABlACAAaAB0AHQAcABzADoALwAvAGEAZABzAGUAcgB2AGkAYwBlAC4AZwBvAG8AZwBsAGUALgBkAGsAIABoAHQAdABwAHMAOgAvAC8AYwByAGUAYQB0AGkAdgBlAGMAbwBtAG0AbwBuAHMALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwBhAGQALgBkAG8AdQBiAGwAZQBjAGwAaQBjAGsALgBuAGUAdAA7ACAAZABlAGYAYQB1AGwAdAAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AOwAgAGYAcgBhAG0AZQAtAHMAcgBjACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUAdABhAGcAbQBhAG4AYQBnAGUAcgAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALQBhAG4AYQBsAHkAdABpAGMAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AeQBvAHUAdAB1AGIAZQAtAG4AbwBjAG8AbwBrAGkAZQAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHQAcgBhAGMAawBlAHIAdABlAHMAdAAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AcwB1AHIAdgBlAHkAZwBpAHoAbQBvAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAYwBjAG8AdQBuAHQAcwAuAGYAaQByAGUAZgBvAHgALgBjAG8AbQAuAGMAbgAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAHkAbwB1AHQAdQBiAGUALgBjAG8AbQA7ACAAcwB0AHkAbABlAC0AcwByAGMAIAAnAHMAZQBsAGYAJwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG4AZQB0ACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBjAG8AbQAgACcAdQBuAHMAYQBmAGUALQBpAG4AbABpAG4AZQAnADsAIABjAG8AbgBuAGUAYwB0AC0AcwByAGMAIAAnAHMAZQBsAGYAJwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG4AZQB0ACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAHQAYQBnAG0AYQBuAGEAZwBlAHIALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC0AYQBuAGEAbAB5AHQAaQBjAHMALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGMAYwBvAHUAbgB0AHMALgBmAGkAcgBlAGYAbwB4AC4AYwBvAG0ALwAgAGgAdAB0AHAAcwA6AC8ALwBhAGMAYwBvAHUAbgB0AHMALgBmAGkAcgBlAGYAbwB4AC4AYwBvAG0ALgBjAG4ALwA7ACAAYwBoAGkAbABkAC0AcwByAGMAIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQB0AGEAZwBtAGEAbgBhAGcAZQByAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQAtAGEAbgBhAGwAeQB0AGkAYwBzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgB5AG8AdQB0AHUAYgBlAC0AbgBvAGMAbwBvAGsAaQBlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdAByAGEAYwBrAGUAcgB0AGUAcwB0AC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBzAHUAcgB2AGUAeQBnAGkAegBtAG8ALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGMAYwBvAHUAbgB0AHMALgBmAGkAcgBlAGYAbwB4AC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtAC4AYwBuACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AeQBvAHUAdAB1AGIAZQAuAGMAbwBtAAA=",
      "output": {
        "cspJSON": "{\"csp-policies\":[{\"child-src\":[\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://www.youtube-nocookie.com\",\"https://trackertest.org\",\"https://www.surveygizmo.com\",\"https://accounts.firefox.com\",\"https://accounts.firefox.com.cn\",\"https://www.youtube.com\"],\"connect-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://accounts.firefox.com/\",\"https://accounts.firefox.com.cn/\"],\"default-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\"],\"frame-src\":[\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://www.youtube-nocookie.com\",\"https://trackertest.org\",\"https://www.surveygizmo.com\",\"https://accounts.firefox.com\",\"https://accounts.firefox.com.cn\",\"https://www.youtube.com\"],\"img-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"data:\",\"https://mozilla.org\",\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://adservice.google.com\",\"https://adservice.google.de\",\"https://adservice.google.dk\",\"https://creativecommons.org\",\"https://ad.doubleclick.net\"],\"report-only\":false,\"script-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"'unsafe-inline'\",\"'unsafe-eval'\",\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://tagmanager.google.com\",\"https://www.youtube.com\",\"https://s.ytimg.com\"],\"style-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"'unsafe-inline'\"]}]}",
        "URISpec": "https://www.mozilla.org/en-US/",
        "originAttributes": {
          "firstPartyDomain": "",
          "inIsolatedMozBrowser": false,
          "privateBrowsingId": 0,
          "userContextId": 0,
        },
      },
    },
    {
      "input": "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAbsBAAAAL2h0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2VuLVVTL2ZpcmVmb3gvYWNjb3VudHMvAAAAAAAAAAUAAAAIAAAADwAAAAj/////AAAACP////8AAAAIAAAADwAAABcAAAAYAAAAFwAAABgAAAAXAAAAGAAAAC8AAAAAAAAAL/////8AAAAA/////wAAABf/////AAAAF/////8BAAAAAAAAAAAAAAABCdntGuXUQAS/4CfOuSPZrLPEwK69Xkyth+CNIQ27P58B3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAbsBAAAAL2h0dHBzOi8vd3d3Lm1vemlsbGEub3JnL2VuLVVTL2ZpcmVmb3gvYWNjb3VudHMvAAAAAAAAAAUAAAAIAAAADwAAAAj/////AAAACP////8AAAAIAAAADwAAABcAAAAYAAAAFwAAABgAAAAXAAAAGAAAAC8AAAAAAAAAL/////8AAAAA/////wAAABf/////AAAAF/////8BAAAAAAAAAAAAAQAABbYAcwBjAHIAaQBwAHQALQBzAHIAYwAgACcAcwBlAGwAZgAnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbgBlAHQAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAGMAbwBtACAAJwB1AG4AcwBhAGYAZQAtAGkAbgBsAGkAbgBlACcAIAAnAHUAbgBzAGEAZgBlAC0AZQB2AGEAbAAnACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUAdABhAGcAbQBhAG4AYQBnAGUAcgAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALQBhAG4AYQBsAHkAdABpAGMAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHQAYQBnAG0AYQBuAGEAZwBlAHIALgBnAG8AbwBnAGwAZQAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AeQBvAHUAdAB1AGIAZQAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHMALgB5AHQAaQBtAGcALgBjAG8AbQA7ACAAaQBtAGcALQBzAHIAYwAgACcAcwBlAGwAZgAnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbgBlAHQAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAGMAbwBtACAAZABhAHQAYQA6ACAAaAB0AHQAcABzADoALwAvAG0AbwB6AGkAbABsAGEALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAHQAYQBnAG0AYQBuAGEAZwBlAHIALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC0AYQBuAGEAbAB5AHQAaQBjAHMALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGQAcwBlAHIAdgBpAGMAZQAuAGcAbwBvAGcAbABlAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBkAHMAZQByAHYAaQBjAGUALgBnAG8AbwBnAGwAZQAuAGQAZQAgAGgAdAB0AHAAcwA6AC8ALwBhAGQAcwBlAHIAdgBpAGMAZQAuAGcAbwBvAGcAbABlAC4AZABrACAAaAB0AHQAcABzADoALwAvAGMAcgBlAGEAdABpAHYAZQBjAG8AbQBtAG8AbgBzAC4AbwByAGcAIABoAHQAdABwAHMAOgAvAC8AYQBkAC4AZABvAHUAYgBsAGUAYwBsAGkAYwBrAC4AbgBlAHQAOwAgAGQAZQBmAGEAdQBsAHQALQBzAHIAYwAgACcAcwBlAGwAZgAnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AbgBlAHQAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAGMAbwBtADsAIABmAHIAYQBtAGUALQBzAHIAYwAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAHQAYQBnAG0AYQBuAGEAZwBlAHIALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAGcAbwBvAGcAbABlAC0AYQBuAGEAbAB5AHQAaQBjAHMALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAHkAbwB1AHQAdQBiAGUALQBuAG8AYwBvAG8AawBpAGUALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwB0AHIAYQBjAGsAZQByAHQAZQBzAHQALgBvAHIAZwAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAHMAdQByAHYAZQB5AGcAaQB6AG0AbwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAYwBjAG8AdQBuAHQAcwAuAGYAaQByAGUAZgBvAHgALgBjAG8AbQAgAGgAdAB0AHAAcwA6AC8ALwBhAGMAYwBvAHUAbgB0AHMALgBmAGkAcgBlAGYAbwB4AC4AYwBvAG0ALgBjAG4AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgB5AG8AdQB0AHUAYgBlAC4AYwBvAG0AOwAgAHMAdAB5AGwAZQAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIAAnAHUAbgBzAGEAZgBlAC0AaQBuAGwAaQBuAGUAJwA7ACAAYwBvAG4AbgBlAGMAdAAtAHMAcgBjACAAJwBzAGUAbABmACcAIABoAHQAdABwAHMAOgAvAC8AKgAuAG0AbwB6AGkAbABsAGEALgBuAGUAdAAgAGgAdAB0AHAAcwA6AC8ALwAqAC4AbQBvAHoAaQBsAGwAYQAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvACoALgBtAG8AegBpAGwAbABhAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQB0AGEAZwBtAGEAbgBhAGcAZQByAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AdwB3AHcALgBnAG8AbwBnAGwAZQAtAGEAbgBhAGwAeQB0AGkAYwBzAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtAC8AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtAC4AYwBuAC8AOwAgAGMAaABpAGwAZAAtAHMAcgBjACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUAdABhAGcAbQBhAG4AYQBnAGUAcgAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AZwBvAG8AZwBsAGUALQBhAG4AYQBsAHkAdABpAGMAcwAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AeQBvAHUAdAB1AGIAZQAtAG4AbwBjAG8AbwBrAGkAZQAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAHQAcgBhAGMAawBlAHIAdABlAHMAdAAuAG8AcgBnACAAaAB0AHQAcABzADoALwAvAHcAdwB3AC4AcwB1AHIAdgBlAHkAZwBpAHoAbQBvAC4AYwBvAG0AIABoAHQAdABwAHMAOgAvAC8AYQBjAGMAbwB1AG4AdABzAC4AZgBpAHIAZQBmAG8AeAAuAGMAbwBtACAAaAB0AHQAcABzADoALwAvAGEAYwBjAG8AdQBuAHQAcwAuAGYAaQByAGUAZgBvAHgALgBjAG8AbQAuAGMAbgAgAGgAdAB0AHAAcwA6AC8ALwB3AHcAdwAuAHkAbwB1AHQAdQBiAGUALgBjAG8AbQAA",
      "output": {
        "URISpec": "https://www.mozilla.org/en-US/firefox/accounts/",
        "originAttributes": {
          "firstPartyDomain": "",
          "inIsolatedMozBrowser": false,
          "privateBrowsingId": 0,
          "userContextId": 0,
        },
        "cspJSON": "{\"csp-policies\":[{\"child-src\":[\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://www.youtube-nocookie.com\",\"https://trackertest.org\",\"https://www.surveygizmo.com\",\"https://accounts.firefox.com\",\"https://accounts.firefox.com.cn\",\"https://www.youtube.com\"],\"connect-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://accounts.firefox.com/\",\"https://accounts.firefox.com.cn/\"],\"default-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\"],\"frame-src\":[\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://www.youtube-nocookie.com\",\"https://trackertest.org\",\"https://www.surveygizmo.com\",\"https://accounts.firefox.com\",\"https://accounts.firefox.com.cn\",\"https://www.youtube.com\"],\"img-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"data:\",\"https://mozilla.org\",\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://adservice.google.com\",\"https://adservice.google.de\",\"https://adservice.google.dk\",\"https://creativecommons.org\",\"https://ad.doubleclick.net\"],\"report-only\":false,\"script-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"'unsafe-inline'\",\"'unsafe-eval'\",\"https://www.googletagmanager.com\",\"https://www.google-analytics.com\",\"https://tagmanager.google.com\",\"https://www.youtube.com\",\"https://s.ytimg.com\"],\"style-src\":[\"'self'\",\"https://*.mozilla.net\",\"https://*.mozilla.org\",\"https://*.mozilla.com\",\"'unsafe-inline'\"]}]}",
      },
    },
    {
      "input": "ZT4OTT7kRfqycpfCC8AeuAAAAAAAAAAAwAAAAAAAAEYB3pRy0IA0EdOTmQAQS6D9QJIHOlRteE8wkTq4cYEyCMYAAAAC/////wAAAbsBAAAAe2h0dHBzOi8vZGV2ZWxvcGVyLm1vemlsbGEub3JnL2VuLVVTLz91dG1fc291cmNlPXd3dy5tb3ppbGxhLm9yZyZ1dG1fbWVkaXVtPXJlZmVycmFsJnV0bV9jYW1wYWlnbj1uYXYmdXRtX2NvbnRlbnQ9ZGV2ZWxvcGVycwAAAAAAAAAFAAAACAAAABUAAAAA/////wAAAAD/////AAAACAAAABUAAAAdAAAAXgAAAB0AAAAHAAAAHQAAAAcAAAAkAAAAAAAAAAD/////AAAAAP////8AAAAlAAAAVgAAAAD/////AQAAAAAAAAAAAAAAAA==",
      "output": {
        "URISpec": "https://developer.mozilla.org/en-US/?utm_source=www.mozilla.org&utm_medium=referral&utm_campaign=nav&utm_content=developers",
        "originAttributes": {
          "firstPartyDomain": "",
          "inIsolatedMozBrowser": false,
          "privateBrowsingId": 0,
          "userContextId": 0,
        },
        "cspJSON": "{}",
      },
    },
    {
      "input": "SmIS26zLEdO3ZQBgsLbOywAAAAAAAAAAwAAAAAAAAEY=",
      "output": {
        "URI": false,
        "originAttributes": {
          "firstPartyDomain": "",
          "inIsolatedMozBrowser": false,
          "privateBrowsingId": 0,
          "userContextId": 0,
        },
        "cspJSON": "{}",
      },
    },
    {
      "input": "vQZuXxRvRHKDMXv9BbHtkAAAAAAAAAAAwAAAAAAAAEYAAAA4bW96LW51bGxwcmluY2lwYWw6ezA0NWNhMThkLTQzNmMtNDc0NC1iYmI2LWIxYTE1MzY2ZGY3OX0AAAAA",
      "output": {
        "URISpec": "moz-nullprincipal:{045ca18d-436c-4744-bbb6-b1a15366df79}",
        "originAttributes": {
          "firstPartyDomain": "",
          "inIsolatedMozBrowser": false,
          "privateBrowsingId": 0,
          "userContextId": 0,
        },
        "cspJSON": "{}",
      },
    },
  ];

  for (let test of serializedPrincipalsFromFirefox) {
    let principal = E10SUtils.deserializePrincipal(test.input);
    is(principal.cspJSON, test.output.cspJSON, "should have CSP");

    for (let key in principal.originAttributes) {
      is(principal.originAttributes[key], test.output.originAttributes[key], `Ensure value of ${key} is ${test.output.originAttributes[key]}`);
    }

    if ("URI" in test.output && test.output.URI === false) {
      is(principal.URI, null, "Should have not have a URI for system");
    } else {
      is(principal.URI.spec, test.output.URISpec, `Should have spec ${test.output.URISpec}`);
    }
  }
});
