/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {ELLIPSIS} = require("devtools/shared/l10n");

add_task(async function () {
  info("Test short URL linkification JSON started");

  let url = "http://example.com/";
  let tab = await addJsonViewTab("data:application/json," + JSON.stringify([url]));

  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(tab.linkedBrowser, {url}, function ({url}) {
    let {document} = content;

    let link = document.querySelector(".jsonPanelBox .treeTable .treeValueCell a");
    is(link.href, url, "The URL was linkified.");
    is(link.textContent, url, "The full URL is displayed.");

    // Click the label
    document.querySelector(".jsonPanelBox .treeTable .treeLabel").click();
    is(link.href, url, "The link target didn't change.");
    is(link.textContent, url, "The link text didn't change.");
  });
});

add_task(async function () {
  info("Test long URL linkification JSON started");

  let url = "http://example.com/" + "a".repeat(100);
  let tab = await addJsonViewTab("data:application/json," + JSON.stringify([url]));

  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(tab.linkedBrowser, {url, ELLIPSIS}, function ({url, ELLIPSIS}) {
    let croppedUrl = url.slice(0, 24) + ELLIPSIS + url.slice(-24);
    let {document} = content;

    let link = document.querySelector(".jsonPanelBox .treeTable .treeValueCell a");
    is(link.href, url, "The URL was linkified.");
    is(link.textContent, url, "The full URL is displayed.");

    // Click the label, this crops the value.
    document.querySelector(".jsonPanelBox .treeTable .treeLabel").click();
    is(link.href, url, "The link target didn't change.");
    is(link.textContent, croppedUrl, "The link text was cropped.");
  });
});
