/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var data = [
  {
    // singleword.
    wrong: "http://example/",
    fixed: "https://www.example.com/",
  },
  {
    // upgrade protocol.
    wrong: "http://www.example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // no difference.
    wrong: "https://www.example.com/",
    fixed: "https://www.example.com/",
    noProtocolFixup: true,
  },
  {
    // don't add prefix when there's more than one dot.
    wrong: "http://www.example.abc.def/",
    fixed: "https://www.example.abc.def/",
  },
  {
    // http -> https.
    wrong: "http://www.example/",
    fixed: "https://www.example/",
  },
  {
    // domain.com -> https://www.domain.com.
    wrong: "http://example.com/",
    fixed: "https://www.example.com/",
  },
  {
    // example/example... -> https://www.example.com/example/
    wrong: "http://example/example/",
    fixed: "https://www.example.com/example/",
  },
  {
    // example/example/s#q -> www.example.com/example/s#q.
    wrong: "http://example/example/s#q",
    fixed: "https://www.example.com/example/s#q",
  },
  {
    wrong: "http://モジラ.org",
    fixed: "https://www.xn--yck6dwa.org/",
  },
  {
    wrong: "http://モジラ",
    fixed: "https://www.xn--yck6dwa.com/",
  },
  {
    wrong: "http://xn--yck6dwa",
    fixed: "https://www.xn--yck6dwa.com/",
  },
  {
    wrong: "https://モジラ.org",
    fixed: "https://www.xn--yck6dwa.org/",
    noProtocolFixup: true,
  },
  {
    wrong: "https://モジラ",
    fixed: "https://www.xn--yck6dwa.com/",
    noProtocolFixup: true,
  },
  {
    wrong: "https://xn--yck6dwa",
    fixed: "https://www.xn--yck6dwa.com/",
    noProtocolFixup: true,
  },
  {
    // Host is https -> fixup typos of protocol
    wrong: "htp://https://mozilla.org",
    fixed: "http://https//mozilla.org",
    noAlternateURI: true,
  },
  {
    // Host is http -> fixup typos of protocol
    wrong: "ttp://http://mozilla.org",
    fixed: "http://http//mozilla.org",
    noAlternateURI: true,
  },
  {
    // Host is localhost -> fixup typos of protocol
    wrong: "htps://localhost://mozilla.org",
    fixed: "https://localhost//mozilla.org",
    noAlternateURI: true,
  },
  {
    // view-source is not http/https -> error
    wrong: "view-source:http://example/example/example/example",
    reject: true,
    comment: "Scheme should be either http or https",
  },
  {
    // file is not http/https -> error
    wrong: "file://http://example/example/example/example",
    reject: true,
    comment: "Scheme should be either http or https",
  },
  {
    // Protocol is missing -> error
    wrong: "example.com",
    reject: true,
    comment: "Scheme should be either http or https",
  },
  {
    // Empty input -> error
    wrong: "",
    reject: true,
    comment: "Should pass a non-null uri",
  },
];

add_task(async function setup() {
  Services.prefs.setStringPref("browser.fixup.alternate.prefix", "www.");
  Services.prefs.setStringPref("browser.fixup.alternate.suffix", ".com");
  Services.prefs.setStringPref("browser.fixup.alternate.protocol", "https");
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.fixup.alternate.prefix");
    Services.prefs.clearUserPref("browser.fixup.alternate.suffix");
    Services.prefs.clearUserPref("browser.fixup.alternate.protocol");
  });
});

add_task(function test_default_https_pref() {
  for (let item of data) {
    if (item.reject) {
      Assert.throws(
        () => Services.uriFixup.forceHttpFixup(item.wrong),
        /NS_ERROR_FAILURE/,
        item.comment
      );
    } else {
      let {
        fixupChangedProtocol,
        fixupCreatedAlternateURI,
        fixedURI,
      } = Services.uriFixup.forceHttpFixup(item.wrong);
      Assert.equal(fixedURI.spec, item.fixed, "Specs should be the same");
      Assert.equal(
        fixupChangedProtocol,
        !item.noProtocolFixup,
        `fixupChangedProtocol should be ${!item.noAlternateURI}`
      );
      Assert.equal(
        fixupCreatedAlternateURI,
        !item.noAlternateURI,
        `fixupCreatedAlternateURI should be ${!item.limitedFixup}`
      );
    }
  }
});
