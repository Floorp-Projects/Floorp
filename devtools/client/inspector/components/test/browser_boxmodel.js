/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model displays the right values and that it updates when
// the node's style is changed

// Expected values:
var res1 = [
  {
    selector: "#boxmodel-element-size",
    value: "160" + "\u00D7" + "160.117"
  },
  {
    selector: ".boxmodel-size > span",
    value: "100" + "\u00D7" + "100.117"
  },
  {
    selector: ".boxmodel-margin.boxmodel-top > span",
    value: 30
  },
  {
    selector: ".boxmodel-margin.boxmodel-left > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-margin.boxmodel-bottom > span",
    value: 30
  },
  {
    selector: ".boxmodel-margin.boxmodel-right > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-padding.boxmodel-top > span",
    value: 20
  },
  {
    selector: ".boxmodel-padding.boxmodel-left > span",
    value: 20
  },
  {
    selector: ".boxmodel-padding.boxmodel-bottom > span",
    value: 20
  },
  {
    selector: ".boxmodel-padding.boxmodel-right > span",
    value: 20
  },
  {
    selector: ".boxmodel-border.boxmodel-top > span",
    value: 10
  },
  {
    selector: ".boxmodel-border.boxmodel-left > span",
    value: 10
  },
  {
    selector: ".boxmodel-border.boxmodel-bottom > span",
    value: 10
  },
  {
    selector: ".boxmodel-border.boxmodel-right > span",
    value: 10
  },
];

var res2 = [
  {
    selector: "#boxmodel-element-size",
    value: "190" + "\u00D7" + "210"
  },
  {
    selector: ".boxmodel-size > span",
    value: "100" + "\u00D7" + "150"
  },
  {
    selector: ".boxmodel-margin.boxmodel-top > span",
    value: 30
  },
  {
    selector: ".boxmodel-margin.boxmodel-left > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-margin.boxmodel-bottom > span",
    value: 30
  },
  {
    selector: ".boxmodel-margin.boxmodel-right > span",
    value: "auto"
  },
  {
    selector: ".boxmodel-padding.boxmodel-top > span",
    value: 20
  },
  {
    selector: ".boxmodel-padding.boxmodel-left > span",
    value: 20
  },
  {
    selector: ".boxmodel-padding.boxmodel-bottom > span",
    value: 20
  },
  {
    selector: ".boxmodel-padding.boxmodel-right > span",
    value: 50
  },
  {
    selector: ".boxmodel-border.boxmodel-top > span",
    value: 10
  },
  {
    selector: ".boxmodel-border.boxmodel-left > span",
    value: 10
  },
  {
    selector: ".boxmodel-border.boxmodel-bottom > span",
    value: 10
  },
  {
    selector: ".boxmodel-border.boxmodel-right > span",
    value: 10
  },
];

add_task(function* () {
  let style = "div { position: absolute; top: 42px; left: 42px; " +
              "height: 100.111px; width: 100px; border: 10px solid black; " +
              "padding: 20px; margin: 30px auto;}";
  let html = "<style>" + style + "</style><div></div>";

  yield addTab("data:text/html," + encodeURIComponent(html));
  let {inspector, view, testActor} = yield openBoxModelView();
  yield selectNode("div", inspector);

  yield testInitialValues(inspector, view);
  yield testChangingValues(inspector, view, testActor);
});

function* testInitialValues(inspector, view) {
  info("Test that the initial values of the box model are correct");
  let viewdoc = view.doc;

  for (let i = 0; i < res1.length; i++) {
    let elt = viewdoc.querySelector(res1[i].selector);
    is(elt.textContent, res1[i].value,
       res1[i].selector + " has the right value.");
  }
}

function* testChangingValues(inspector, view, testActor) {
  info("Test that changing the document updates the box model");
  let viewdoc = view.doc;

  let onUpdated = waitForUpdate(inspector);
  yield testActor.setAttribute("div", "style",
                               "height:150px;padding-right:50px;");
  yield onUpdated;

  for (let i = 0; i < res2.length; i++) {
    let elt = viewdoc.querySelector(res2[i].selector);
    is(elt.textContent, res2[i].value,
       res2[i].selector + " has the right value after style update.");
  }
}
