/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { validateProfilerWebChannelUrl } = ChromeUtils.importESModule(
  "resource:///modules/DevToolsStartup.sys.mjs"
);

add_task(function test() {
  info(
    "Since the WebChannel can communicate with a content page, test that only " +
      "trusted URLs can be used with this mechanism."
  );

  const { checkUrlIsValid, checkUrlIsInvalid } = setup();

  info("Check all of the valid URLs");
  checkUrlIsValid("https://profiler.firefox.com");
  checkUrlIsValid("http://example.com");
  checkUrlIsValid("http://localhost:4242");
  checkUrlIsValid("http://localhost:32343434");
  checkUrlIsValid("http://localhost:4242/");
  checkUrlIsValid("https://deploy-preview-1234--perf-html.netlify.com");
  checkUrlIsValid("https://deploy-preview-1234--perf-html.netlify.com/");
  checkUrlIsValid("https://deploy-preview-1234--perf-html.netlify.app");
  checkUrlIsValid("https://deploy-preview-1234--perf-html.netlify.app/");
  checkUrlIsValid("https://main--perf-html.netlify.app/");

  info("Check all of the invalid URLs");
  checkUrlIsInvalid("https://profiler.firefox.com/some-other-path");
  checkUrlIsInvalid("http://localhost:4242/some-other-path");
  checkUrlIsInvalid("http://profiler.firefox.com.example.com");
  checkUrlIsInvalid("http://mozilla.com");
  checkUrlIsInvalid("https://deploy-preview-1234--perf-html.netlify.dev");
  checkUrlIsInvalid("https://anything--perf-html.netlify.app/");
});

function setup() {
  function checkUrlIsValid(url) {
    info(`Check that ${url} is valid`);
    equal(
      validateProfilerWebChannelUrl(url),
      url,
      `"${url}" is a valid WebChannel URL.`
    );
  }

  function checkUrlIsInvalid(url) {
    info(`Check that ${url} is invalid`);
    equal(
      validateProfilerWebChannelUrl(url),
      "https://profiler.firefox.com",
      `"${url}" was not valid, and was reset to the base URL.`
    );
  }

  return {
    checkUrlIsValid,
    checkUrlIsInvalid,
  };
}
