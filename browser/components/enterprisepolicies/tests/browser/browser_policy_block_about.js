/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ABOUT_CONTRACT = "@mozilla.org/network/protocol/about;1?what=";

const policiesToTest = [
  {
    policies: {
      BlockAboutAddons: true,
    },
    urls: ["about:addons", "about:ADDONS"],
  },
  {
    policies: {
      BlockAboutConfig: true,
    },
    urls: ["about:config", "about:Config"],
  },
  {
    policies: {
      BlockAboutProfiles: true,
    },
    urls: ["about:profiles", "about:pRofiles"],
  },
  {
    policies: {
      BlockAboutSupport: true,
    },
    urls: ["about:support", "about:suPPort"],
  },
];

add_task(async function testAboutTask() {
  for (let policyToTest of policiesToTest) {
    let policyJSON = { policies: {} };
    policyJSON.policies = policyToTest.policies;
    for (let url of policyToTest.urls) {
      if (url.startsWith("about")) {
        let feature = url.split(":")[1].toLowerCase();
        let aboutModule = Cc[ABOUT_CONTRACT + feature].getService(
          Ci.nsIAboutModule
        );
        let chromeURL = aboutModule.getChromeURI(Services.io.newURI(url)).spec;
        await testPageBlockedByPolicy(chromeURL, policyJSON);
      }
      await testPageBlockedByPolicy(url, policyJSON);
    }
  }
});
