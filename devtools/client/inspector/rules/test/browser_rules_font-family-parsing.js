/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the parsed font-family property value shown in the rules
// pane is correct.

const TEST_URI = `
  <style type="text/css">
    #id1 {
      font-family: georgia, arial, sans-serif;
    }
    #id2 {
      font-family: georgia,arial,sans-serif;
    }
    #id3 {
      font-family: georgia ,arial ,sans-serif;
    }
    #id4 {
      font-family: georgia , arial , sans-serif;
    }
    #id4 {
      font-family:   arial,  georgia,   sans-serif  ;
    }
    #id5 {
      font-family: helvetica !important;
    }
  </style>
  <div id="id1">1</div>
  <div id="id2">2</div>
  <div id="id3">3</div>
  <div id="id4">4</div>
  <div id="id5">5</div>
`;

const TESTS = [
  {selector: "#id1", expectedTextContent: "georgia, arial, sans-serif"},
  {selector: "#id2", expectedTextContent: "georgia,arial,sans-serif"},
  {selector: "#id3", expectedTextContent: "georgia ,arial ,sans-serif"},
  {selector: "#id4", expectedTextContent: "arial, georgia, sans-serif"},
  {selector: "#id5", expectedTextContent: "helvetica !important"},
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = await openRuleView();

  for (let {selector, expectedTextContent} of TESTS) {
    await selectNode(selector, inspector);
    info("Looking for font-family property value in selector " + selector);

    let prop = getRuleViewProperty(view, selector, "font-family").valueSpan;
    is(prop.textContent, expectedTextContent,
       "The font-family property value is correct");
  }
});
