/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that blocked sites are caught by InteractionsBlocklist.
 */

ChromeUtils.defineESModuleGetters(this, {
  InteractionsBlocklist: "resource:///modules/InteractionsBlocklist.sys.mjs",
});

let BLOCKED_URLS = [
  "https://www.bing.com/search?q=mozilla",
  "https://duckduckgo.com/?q=a+test&kp=1&t=ffab",
  "https://www.google.com/search?q=mozilla",
  "https://www.google.ca/search?q=test",
  "https://mozilla.zoom.us/j/123456789",
  "https://yandex.az/search/?text=mozilla",
  "https://www.baidu.com/s?ie=utf-8&f=8&rsv_bp=1&rsv_idx=1&ch=&tn=baidu&bar=&wd=mozilla&rn=&fenlei=256&oq=&rsv_pq=970f2b8f001757b9&rsv_t=1f5d2V2o80HPdZtZnhodwkc7nZXTvDI1zwdPy%2FAeomnvFFGIrU1F3D9WoK4&rqlang=cn",
  "https://accounts.google.com/o/oauth2/v2/auth/identifier/foobar",
  "https://auth.mozilla.auth0.com/login/foobar",
  "https://accounts.google.com/signin/oauth/consent/foobar",
  "https://accounts.google.com/o/oauth2/v2/auth?client_id=ZZZ",
  "https://login.microsoftonline.com/common/oauth2/v2.0/authorize/foobar",
];

let ALLOWED_URLS = [
  "https://example.com",
  "https://zoom.us/pricing",
  "https://www.google.ca/maps/place/Toronto,+ON/@43.7181557,-79.5181414,11z/data=!3m1!4b1!4m5!3m4!1s0x89d4cb90d7c63ba5:0x323555502ab4c477!8m2!3d43.653226!4d-79.3831843",
  "https://example.com/https://auth.mozilla.auth0.com/login/foobar",
];

// Tests that initializing InteractionsBlocklist loads the regexes from the
// customBlocklist pref on initialization. This subtest should always be the
// first one in this file.
add_task(async function blockedOnInit() {
  Services.prefs.setStringPref(
    "places.interactions.customBlocklist",
    '["^(https?:\\\\/\\\\/)?mochi.test"]'
  );
  Assert.ok(
    InteractionsBlocklist.isUrlBlocklisted("https://mochi.test"),
    "mochi.test is blocklisted."
  );
  InteractionsBlocklist.removeRegexFromBlocklist("^(https?:\\/\\/)?mochi.test");
  Assert.ok(
    !InteractionsBlocklist.isUrlBlocklisted("https://mochi.test"),
    "mochi.test is not blocklisted."
  );
});

add_task(async function test() {
  for (let url of BLOCKED_URLS) {
    Assert.ok(
      InteractionsBlocklist.isUrlBlocklisted(url),
      `${url} is blocklisted.`
    );
  }

  for (let url of ALLOWED_URLS) {
    Assert.ok(
      !InteractionsBlocklist.isUrlBlocklisted(url),
      `${url} is not blocklisted.`
    );
  }
});
