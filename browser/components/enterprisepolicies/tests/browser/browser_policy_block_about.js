/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ABOUT_CONTRACT = "@mozilla.org/network/protocol/about;1?what=";

const policiesToTest = [
  {
    policies: {
      BlockAboutAddons: true,
    },
    urls: ["about:addons"],
  },
  {
    policies: {
      BlockAboutConfig: true,
    },
    urls: ["about:config"],
  },
  {
    policies: {
      BlockAboutProfiles: true,
    },
    urls: ["about:profiles"],
  },
  {
    policies: {
      BlockAboutSupport: true,
    },
    urls: ["about:support"],
  },
  {
    policies: {
      DisableDeveloperTools: true,
    },
    urls: [
      "about:devtools",
      "about:debugging",
      "about:devtools-toolbox",
      "about:profiling",
    ],
  },
  {
    policies: {
      DisablePrivateBrowsing: true,
    },
    urls: ["about:privatebrowsing"],
  },
  {
    policies: {
      DisableTelemetry: true,
    },
    urls: ["about:telemetry"],
  },
  {
    policies: {
      PasswordManagerEnabled: false,
    },
    urls: ["about:logins"],
  },
];

add_task(async function testAboutTask() {
  for (let policyToTest of policiesToTest) {
    let policyJSON = { policies: {} };
    policyJSON.policies = policyToTest.policies;
    for (let url of policyToTest.urls) {
      if (url.startsWith("about")) {
        let feature = url.split(":")[1];
        let aboutModule = Cc[ABOUT_CONTRACT + feature].getService(
          Ci.nsIAboutModule
        );
        let chromeURL = aboutModule.getChromeURI(Services.io.newURI(url)).spec;
        await testPageBlockedByPolicy(policyJSON, chromeURL);
      }
      await testPageBlockedByPolicy(policyJSON, url);
    }
  }
});

async function testPageBlockedByPolicy(policyJSON, page) {
  await EnterprisePolicyTesting.setupPolicyEngineWithJson(policyJSON);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      BrowserTestUtils.loadURI(browser, page);
      await BrowserTestUtils.browserLoaded(browser, false, page, true);
      await SpecialPowers.spawn(browser, [page], async function(innerPage) {
        ok(
          content.document.documentURI.startsWith(
            "about:neterror?e=blockedByPolicy"
          ),
          content.document.documentURI +
            " should start with about:neterror?e=blockedByPolicy"
        );
      });
    }
  );
}
