/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the computed-view keyboard navigation

let test = asyncTest(function*() {
  yield addTab("data:text/html,computed view keyboard nav test");

  content.document.body.innerHTML = '<style type="text/css"> ' +
    'span { font-variant: small-caps; color: #000000; } ' +
    '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="font-size: 12pt">hi.</p>\n' +
    '<p id="body" style="font-size: 12pt">I am a test-case. This text exists ' +
    'solely to provide some things to <span style="color: yellow">' +
    'highlight</span> and <span style="font-weight: bold">count</span> ' +
    'style list-items in the box at right. If you are reading this, ' +
    'you should go do something else instead. Maybe read a book. Or better ' +
    'yet, write some test-cases for another bit of code. ' +
    '<span style="font-style: italic">some text</span></p>\n' +
    '<p id="closing">more text</p>\n' +
    '<p>even more text</p>' +
    '</div>';
  content.document.title = "Computed view keyboard navigation test";

  info("Opening the computed-view");
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test node");
  yield selectNode("span", inspector);

  info("Selecting the first computed style in the list");
  let firstStyle = view.styleDocument.querySelector(".property-view");
  ok(firstStyle, "First computed style found in panel");
  firstStyle.focus();

  info("Tab to select the 2nd style and press return");
  let onExpanded = inspector.once("computed-view-property-expanded");
  EventUtils.synthesizeKey("VK_TAB", {});
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onExpanded;

  info("Verify the 2nd style has been expanded");
  let secondStyleSelectors = view.styleDocument.querySelectorAll(
    ".property-content .matchedselectors")[1];
  ok(secondStyleSelectors.childNodes.length > 0, "Matched selectors expanded");

  info("Tab back up and test the same thing, with space");
  let onExpanded = inspector.once("computed-view-property-expanded");
  EventUtils.synthesizeKey("VK_TAB", {shiftKey: true});
  EventUtils.synthesizeKey("VK_SPACE", {});
  yield onExpanded;

  info("Verify the 1st style has been expanded too");
  let firstStyleSelectors = view.styleDocument.querySelectorAll(
    ".property-content .matchedselectors")[0];
  ok(firstStyleSelectors.childNodes.length > 0, "Matched selectors expanded");
});
