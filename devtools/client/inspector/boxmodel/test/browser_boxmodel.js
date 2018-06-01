/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the box model displays the right values and that it updates when
// the node's style is changed

// Expected values:
var res1 = [
  {
    selector: ".boxmodel-element-size",
    value: "160" + "\u00D7" + "160.117"
  },
  {
    selector: ".boxmodel-size > .boxmodel-width",
    value: "100"
  },
  {
    selector: ".boxmodel-size > .boxmodel-height",
    value: "100.117"
  },
  {
    selector: ".boxmodel-position.boxmodel-top > span",
    value: 42
  },
  {
    selector: ".boxmodel-position.boxmodel-left > span",
    value: 42
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
    selector: ".boxmodel-element-size",
    value: "190" + "\u00D7" + "210"
  },
  {
    selector: ".boxmodel-size > .boxmodel-width",
    value: "100"
  },
  {
    selector: ".boxmodel-size > .boxmodel-height",
    value: "150"
  },
  {
    selector: ".boxmodel-position.boxmodel-top > span",
    value: 50
  },
  {
    selector: ".boxmodel-position.boxmodel-left > span",
    value: 42
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

add_task(async function() {
  const style = "div { position: absolute; top: 42px; left: 42px; " +
              "height: 100.111px; width: 100px; border: 10px solid black; " +
              "padding: 20px; margin: 30px auto;}";
  const html = "<style>" + style + "</style><div></div>";

  await addTab("data:text/html," + encodeURIComponent(html));
  const {inspector, boxmodel, testActor} = await openLayoutView();
  await selectNode("div", inspector);

  await testInitialValues(inspector, boxmodel);
  await testChangingValues(inspector, boxmodel, testActor);
});

function testInitialValues(inspector, boxmodel) {
  info("Test that the initial values of the box model are correct");
  const doc = boxmodel.document;

  for (let i = 0; i < res1.length; i++) {
    const elt = doc.querySelector(res1[i].selector);
    is(elt.textContent, res1[i].value,
       res1[i].selector + " has the right value.");
  }
}

async function testChangingValues(inspector, boxmodel, testActor) {
  info("Test that changing the document updates the box model");
  const doc = boxmodel.document;

  const onUpdated = waitForUpdate(inspector);
  await testActor.setAttribute("div", "style",
                               "height:150px;padding-right:50px;top:50px");
  await onUpdated;

  for (let i = 0; i < res2.length; i++) {
    const elt = doc.querySelector(res2[i].selector);
    is(elt.textContent, res2[i].value,
       res2[i].selector + " has the right value after style update.");
  }
}
