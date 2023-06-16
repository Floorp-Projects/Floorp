/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function checkArgs(message, expect, prefs = {}) {
  info(`Setting prefs: ${JSON.stringify(prefs)}`);
  await SpecialPowers.pushPrefEnv({
    set: Object.entries(prefs).map(keyVal => {
      if (typeof keyVal[1] == "object") {
        keyVal[1] = JSON.stringify(keyVal[1]);
      }
      return keyVal;
    }),
  });

  // Check the defaultArgs for startup behavior
  Assert.equal(
    Cc["@mozilla.org/browser/clh;1"]
      .getService(Ci.nsIBrowserHandler)
      .wrappedJSObject.getArgs(true),
    expect,
    message
  );
}

add_task(async function test_once_expire() {
  const url = "https://www.mozilla.org/";
  await checkArgs("no expiration", url, {
    "browser.startup.homepage_override.once": { url },
  });

  await checkArgs("expired", "about:blank", {
    "browser.startup.homepage_override.once": { expire: 0, url },
  });

  await checkArgs("not expired", url, {
    "browser.startup.homepage_override.once": { expire: Date.now() * 2, url },
  });
});

add_task(async function test_once_invalid() {
  await checkArgs("not json", "about:blank", {
    "browser.startup.homepage_override.once": "https://not.json",
  });

  await checkArgs("not string", "about:blank", {
    "browser.startup.homepage_override.once": { url: 5 },
  });

  await checkArgs("not https", "about:blank", {
    "browser.startup.homepage_override.once": {
      url: "http://www.mozilla.org/",
    },
  });

  await checkArgs("not portless", "about:blank", {
    "browser.startup.homepage_override.once": {
      url: "https://www.mozilla.org:123/",
    },
  });

  await checkArgs("invalid protocol", "about:blank", {
    "browser.startup.homepage_override.once": {
      url: "data:text/plain,hello world",
    },
  });

  await checkArgs("invalid domain", "about:blank", {
    "browser.startup.homepage_override.once": {
      url: "https://wwwmozilla.org/",
    },
  });

  await checkArgs(
    "invalid second domain",
    "https://valid.firefox.com/|https://mozilla.org/",
    {
      "browser.startup.homepage_override.once": {
        url: "https://valid.firefox.com|https://invalidfirefox.com|https://mozilla.org",
      },
    }
  );
});

add_task(async function test_once() {
  await checkArgs("initial test prefs (no homepage)", "about:blank");

  const url = "https://www.mozilla.org/";
  await checkArgs("override once", url, {
    "browser.startup.homepage_override.once": { url },
  });

  await checkArgs("once cleared", "about:blank");

  await checkArgs("formatted", "https://www.mozilla.org/en-US", {
    "browser.startup.homepage_override.once": {
      url: "https://www.mozilla.org/%LOCALE%",
    },
  });

  await checkArgs("use homepage", "about:home", {
    "browser.startup.page": 1,
  });

  await checkArgs("once with homepage", `${url}|about:home`, {
    "browser.startup.homepage_override.once": { url },
  });

  await checkArgs("once cleared again", "about:home");

  await checkArgs("prefer major version override", `about:welcome|about:home`, {
    "browser.startup.homepage_override.mstone": "1.0",
    "browser.startup.homepage_override.once": { url },
    "startup.homepage_override_url": "about:welcome",
  });

  await checkArgs("once after major", `${url}|about:home`);

  await checkArgs("once cleared yet again", "about:home");
});
