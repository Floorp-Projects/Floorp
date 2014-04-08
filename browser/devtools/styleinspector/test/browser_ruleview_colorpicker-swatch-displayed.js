/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that color swatches are displayed next to colors in the rule-view

let ruleView;

const PAGE_CONTENT = [
  '<style type="text/css">',
  '  body {',
  '    color: red;',
  '    background-color: #ededed;',
  '    background-image: url(chrome://global/skin/icons/warning-64.png);',
  '    border: 2em solid rgba(120, 120, 120, .5);',
  '  }',
  '</style>',
  'Testing the color picker tooltip!'
].join("\n");

function test() {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function load(evt) {
    gBrowser.selectedBrowser.removeEventListener("load", load, true);
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,rule view color picker tooltip test";
}

function createDocument() {
  content.document.body.innerHTML = PAGE_CONTENT;

  openRuleView((inspector, view) => {
    ruleView = view;
    inspector.once("inspector-updated", testColorSwatchesAreDisplayed);
  });
}

function testColorSwatchesAreDisplayed() {
  let cSwatch = getRuleViewProperty("color", ruleView).valueSpan
    .querySelector(".ruleview-colorswatch");
  ok(cSwatch, "Color swatch is displayed for the color property");

  let bgSwatch = getRuleViewProperty("background-color", ruleView).valueSpan
    .querySelector(".ruleview-colorswatch");
  ok(bgSwatch, "Color swatch is displayed for the bg-color property");

  let bSwatch = getRuleViewProperty("border", ruleView).valueSpan
    .querySelector(".ruleview-colorswatch");
  ok(bSwatch, "Color swatch is displayed for the border property");

  ruleView = null;
  finish();
}
