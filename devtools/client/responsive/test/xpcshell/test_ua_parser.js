/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for user agent parser.

const { parseUserAgent } = require("devtools/client/responsive/utils/ua");

const TEST_DATA = [
  {
    userAgent:
      "Mozilla/5.0 (Android 4.4.3; Tablet; rv:41.0) Gecko/41.0 Firefox/41.0",
    expectedBrowser: { name: "Firefox", version: "41" },
    expectedOS: { name: "Android", version: "4.4.3" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Android 8.0.0; Mobile; rv:70.0) Gecko/70.0 Firefox/70.0",
    expectedBrowser: { name: "Firefox", version: "70" },
    expectedOS: { name: "Android", version: "8" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:54.0) Gecko/20100101 Firefox/70.1",
    expectedBrowser: { name: "Firefox", version: "70.1" },
    expectedOS: { name: "Windows NT", version: "6.1" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.13; rv:61.0) Gecko/20100101 Firefox/70.0",
    expectedBrowser: { name: "Firefox", version: "70" },
    expectedOS: { name: "Mac OSX", version: "10.13" },
  },
  {
    userAgent:
      "Mozilla/5.0 (X11; Linux i586; rv:31.0) Gecko/20100101 Firefox/70.0",
    expectedBrowser: { name: "Firefox", version: "70" },
    expectedOS: { name: "Linux", version: null },
  },
  {
    userAgent:
      "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) FxiOS/13.2b11866 Mobile/16A366 Safari/605.1.15",
    expectedBrowser: { name: "Firefox", version: "13.2b11866" },
    expectedOS: { name: "iOS", version: "12" },
  },
  {
    userAgent:
      "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/12.0 Mobile/15A372 Safari/604.1",
    expectedBrowser: { name: "Safari", version: "12" },
    expectedOS: { name: "iOS", version: "12" },
  },
  {
    userAgent:
      "Mozilla/5.0 (iPad; CPU OS 14_7_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.1.2 Mobile/15E148 Safari/604.1",
    expectedBrowser: { name: "Safari", version: "14.1" },
    expectedOS: { name: "iPadOS", version: "14.7.1" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_6) AppleWebKit/601.7.8 (KHTML, like Gecko) Version/9.1.2 Safari/601.7.7",
    expectedBrowser: { name: "Safari", version: "9.1" },
    expectedOS: { name: "Mac OSX", version: "10.11.6" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Linux; Android 8.0.0; Nexus 6P Build/OPP3.170518.006) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/67.0.3396.87 Mobile Safari/537.36",
    expectedBrowser: { name: "Chrome", version: "67" },
    expectedOS: { name: "Android", version: "8" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Linux; Android 11; SAMSUNG SM-G973U) AppleWebKit/537.36 (KHTML, like Gecko) SamsungBrowser/14.2 Chrome/87.0.4280.141 Mobile Safari/537.36",
    expectedBrowser: { name: "Chrome", version: "87" },
    expectedOS: { name: "Android", version: "11" },
  },
  {
    userAgent:
      "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/69.0.3497.105 Mobile/15E148 Safari/605.1",
    expectedBrowser: { name: "Chrome", version: "69" },
    expectedOS: { name: "iOS", version: "12" },
  },
  {
    userAgent:
      "Mozilla/5.0 (X11; CrOS x86_64 11895.118.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.159 Safari/537.36",
    expectedBrowser: { name: "Chrome", version: "74" },
    expectedOS: { name: "Chrome OS", version: "11895.118" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Windows Phone 10.0; Android 4.2.1; Microsoft; Lumia 950) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2486.0 Mobile Safari/537.36 Edge/14.14263",
    expectedBrowser: { name: "Edge", version: "14.14263" },
    expectedOS: { name: "Windows Phone", version: "10.0" },
  },
  {
    userAgent:
      "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36 OPR/43.0.2442.991",
    expectedBrowser: { name: "Opera", version: "43" },
    expectedOS: { name: "Windows NT", version: "10.0" },
  },
  {
    userAgent:
      "Opera/9.80 (Linux armv7l) Presto/2.12.407 Version/12.51 , D50u-D1-UHD/V1.5.16-UHD (Vizio, D50u-D1, Wireless)",
    expectedBrowser: { name: "Opera", version: "9.80" },
    expectedOS: { name: "Linux", version: null },
  },
  {
    userAgent:
      "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 1.1.4322)",
    expectedBrowser: { name: "IE", version: "6" },
    expectedOS: { name: "Windows NT", version: "5.1" },
  },
  {
    userAgent: "1080p Full HD Television",
    expectedBrowser: null,
    expectedOS: null,
  },
];

add_task(async function() {
  for (const { userAgent, expectedBrowser, expectedOS } of TEST_DATA) {
    info(`Test for ${userAgent}`);
    const { browser, os } = parseUserAgent(userAgent);
    deepEqual(browser, expectedBrowser, "Parsed browser is correct");
    deepEqual(os, expectedOS, "Parsed OS is correct");
  }
});
