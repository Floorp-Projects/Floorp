/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PAGE_URL = URL_ROOT + "page_basic.html";
const JSON_XHR_URL = URL_ROOT + "test.json";

const plainPostBody = "test-data";
const jsonData = "{\"bar\": \"baz\"}";
const jsonRendered = "bar\"baz\"";
const xmlPostBody = "<xml><name>John</name></xml>";

/**
 * This test generates XHR requests in the page, expands
 * networks details in the Console panel and checks that
 * Post data are properly rendered.
 */
add_task(function* () {
  info("Test XHR Spy post plain body started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "POST",
    url: JSON_XHR_URL,
    body: plainPostBody
  });

  // Check post body data
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "post");
  let postContent = tabBody.querySelector(
    ".netInfoGroup.raw.opened .netInfoGroupContent");
  is(postContent.textContent, plainPostBody,
    "Post body must be properly rendered");
});

add_task(function* () {
  info("Test XHR Spy post JSON body started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "POST",
    url: JSON_XHR_URL,
    body: jsonData,
    requestHeaders: [{
      name: "Content-Type",
      value: "application/json"
    }]
  });

  // Check post body data
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "post");
  let postContent = tabBody.querySelector(
    ".netInfoGroup.json.opened .netInfoGroupContent");
  is(postContent.textContent, jsonRendered,
    "Post body must be properly rendered");

  let rawPostContent = tabBody.querySelector(
    ".netInfoGroup.raw.opened .netInfoGroupContent");
  ok(!rawPostContent, "Raw response group must be collapsed");
});

add_task(function* () {
  info("Test XHR Spy post XML body started");

  let {hud} = yield addTestTab(TEST_PAGE_URL);

  let netInfoBody = yield executeAndInspectXhr(hud, {
    method: "POST",
    url: JSON_XHR_URL,
    body: xmlPostBody,
    requestHeaders: [{
      name: "Content-Type",
      value: "application/xml"
    }]
  });

  // Check post body data
  let tabBody = yield selectNetInfoTab(hud, netInfoBody, "post");
  let rawPostContent = tabBody.querySelector(
    ".netInfoGroup.raw.opened .netInfoGroupContent");
  is(rawPostContent.textContent, xmlPostBody,
    "Raw response group must not be collapsed");
});
