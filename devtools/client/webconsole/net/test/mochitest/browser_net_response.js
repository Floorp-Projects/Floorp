/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";
const TEXT_XHR_URL = URL_ROOT + "test.txt";
const JSON_XHR_URL = URL_ROOT + "test.json";
const XML_XHR_URL = URL_ROOT + "test.xml";

const textResponseBody = "this is a response";
const jsonResponseBody = "name\"John\"";

// Individual tests below generate XHR request in the page, expand
// network details in the Console panel and checks various types
// of response bodies.

/**
 * Validate plain text response
 */
add_task(function* () {
  info("Test XHR Spy respone plain body started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: TEXT_XHR_URL,
  });

  // Check response body data
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "response");
  let responseContent = tabBody.querySelector(
    ".netInfoGroup.raw.opened .netInfoGroupContent");

  ok(responseContent.textContent.indexOf(textResponseBody) > -1,
    "Response body must be properly rendered");
});

/**
 * Validate XML response
 */
add_task(function* () {
  info("Test XHR Spy response XML body started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: XML_XHR_URL,
  });

  // Check response body data
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "response");
  let rawResponseContent = tabBody.querySelector(
    ".netInfoGroup.raw.opened .netInfoGroupContent");
  ok(rawResponseContent, "Raw response group must not be collapsed");
});

/**
 * Validate JSON response
 */
add_task(function* () {
  info("Test XHR Spy response JSON body started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "GET",
    url: JSON_XHR_URL,
  });

  // Check response body data
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "response");
  let responseContent = tabBody.querySelector(
    ".netInfoGroup.json .netInfoGroupContent");

  is(responseContent.textContent, jsonResponseBody,
    "Response body must be properly rendered");

  let rawResponseContent = tabBody.querySelector(
    ".netInfoGroup.raw.opened .netInfoGroupContent");
  ok(!rawResponseContent, "Raw response group must be collapsed");
});
