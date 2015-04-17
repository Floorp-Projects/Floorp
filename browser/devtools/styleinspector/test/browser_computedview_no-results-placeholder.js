/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the no results placeholder works properly.

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,no results placeholder test");

  info("Creating the test document");
  content.document.body.innerHTML = '<style type="text/css"> ' +
    '.matches {color: #F00;}</style>' +
    '<span id="matches" class="matches">Some styled text</span>';
  content.document.title = "Tests that the no results placeholder works properly";

  info("Opening the computed view");
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test node");
  yield selectNode("#matches", inspector);

  yield enterInvalidFilter(inspector, view);
  checkNoResultsPlaceholderShown(view);

  yield clearFilterText(inspector, view);
  checkNoResultsPlaceholderHidden(view);
});

function* enterInvalidFilter(inspector, computedView) {
  let searchbar = computedView.searchField;
  let searchTerm = "xxxxx";

  info("setting filter text to \"" + searchTerm + "\"");

  let onRefreshed = inspector.once("computed-view-refreshed");
  searchbar.focus();
  synthesizeKeys(searchTerm, computedView.styleWindow)
  yield onRefreshed;
}

function checkNoResultsPlaceholderShown(computedView) {
  info("Checking that the no results placeholder is shown");

  let placeholder = computedView.noResults;
  let win = computedView.styleWindow;
  let display = win.getComputedStyle(placeholder).display;
  is(display, "block", "placeholder is visible");
}

function* clearFilterText(inspector, computedView) {
  info("Clearing the filter text");

  let searchbar = computedView.searchField;

  let onRefreshed = inspector.once("computed-view-refreshed");
  searchbar.focus();
  searchbar.value = "";
  EventUtils.synthesizeKey("c", {}, computedView.styleWindow);
  yield onRefreshed;
}

function checkNoResultsPlaceholderHidden(computedView) {
  info("Checking that the no results placeholder is hidden");

  let placeholder = computedView.noResults;
  let win = computedView.styleWindow;
  let display = win.getComputedStyle(placeholder).display;
  is(display, "none", "placeholder is hidden");
}
