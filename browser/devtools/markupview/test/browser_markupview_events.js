/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = TEST_URL_ROOT + "doc_markup_events.html";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  yield inspector.markup.expandAll();

  yield checkEventsForNode("html", [
    {
      type: "load",
      filename: TEST_URL,
      bubbling: true,
      dom0: true,
      handler: "init();"
    }
  ]);

  yield checkEventsForNode("#container", [
    {
      type: "mouseover",
      filename: TEST_URL + ":62",
      bubbling: false,
      dom0: false,
      handler: 'function mouseoverHandler(event) {\n' +
               '  if (event.target.id !== "container") {\n' +
               '    let output = document.getElementById("output");\n' +
               '    output.textContent = event.target.textContent;\n' +
               '  }\n' +
               '}'
    }
  ]);

  yield checkEventsForNode("#multiple", [
    {
      type: "click",
      filename: TEST_URL + ":69",
      bubbling: true,
      dom0: false,
      handler: 'function clickHandler(event) {\n' +
               '  let output = document.getElementById("output");\n' +
               '  output.textContent = "click";\n' +
               '}'
    },
    {
      type: "mouseup",
      filename: TEST_URL + ":78",
      bubbling: true,
      dom0: false,
      handler: 'function mouseupHandler(event) {\n' +
               '  let output = document.getElementById("output");\n' +
               '  output.textContent = "mouseup";\n' +
               '}'
    }
  ]);

  yield checkEventsForNode("#DOM0", [
    {
      type: "click",
      filename: TEST_URL,
      bubbling: true,
      dom0: true,
      handler: "alert('hi')"
    }
  ]);

  yield checkEventsForNode("#handleevent", [
    {
      type: "click",
      filename: TEST_URL + ":89",
      bubbling: true,
      dom0: false,
      handler: 'handleEvent: function(blah) {\n' +
               '  alert("handleEvent clicked");\n' +
               '}'
    }
  ]);

  yield checkEventsForNode("#fatarrow", [
    {
      type: "click",
      filename: TEST_URL + ":57",
      bubbling: true,
      dom0: false,
      handler: 'event => {\n' +
               '  alert("Yay for the fat arrow!");\n' +
               '}'
    }
  ]);

  yield checkEventsForNode("#boundhe", [
    {
      type: "click",
      filename: TEST_URL + ":101",
      bubbling: true,
      dom0: false,
      handler: 'handleEvent: function() {\n' +
               '  alert("boundHandleEvent clicked");\n' +
               '}'
    }
  ]);

  yield checkEventsForNode("#bound", [
    {
      type: "click",
      filename: TEST_URL + ":74",
      bubbling: true,
      dom0: false,
      handler: 'function boundClickHandler(event) {\n' +
               '  alert("Bound event clicked");\n' +
               '}'
    }
  ]);

  gBrowser.removeCurrentTab();

  // Wait for promises to avoid leaks when running this as a single test.
  // We need to do this because we have opened a bunch of popups and don't them
  // to affect other test runs when they are GCd.
  yield promiseNextTick();

  function* checkEventsForNode(selector, expected) {
    let container = yield getContainerForSelector(selector, inspector);
    let evHolder = container.elt.querySelector(".markupview-events");
    let tooltip = inspector.markup.tooltip;

    yield selectNode(selector, inspector);

    // Click button to show tooltip
    info("Clicking evHolder");
    EventUtils.synthesizeMouseAtCenter(evHolder, {}, inspector.markup.doc.defaultView);
    yield tooltip.once("shown");
    info("tooltip shown");

    // Check values
    let content = tooltip.content;
    let result = content.querySelectorAll("label,.event-tooltip-content-box");
    let nodeFront = container.node;
    let cssSelector = nodeFront.nodeName + "#" + nodeFront.id;

    let out = [];

    for (let i = 0; i < result.length;) {
      let type = result[i++];
      let filename = result[i++];
      let bubbling = result[i++];
      let dom0 = result[i++];
      let content = result[i++];

      EventUtils.synthesizeMouseAtCenter(type, {}, type.ownerGlobal);

      yield tooltip.once("event-tooltip-ready");

      let editor = tooltip.eventEditors.get(content).editor;

      out.push({
        type: type.getAttribute("value"),
        filename: filename.getAttribute("value"),
        bubbling: bubbling.getAttribute("value") === "Bubbling",
        dom0: dom0.getAttribute("value") === "DOM0",
        handler: editor.getText()
      });
    }

    for (let i = 0; i < out.length; i++) {
      is(out[i].type, expected[i].type, "type matches for " + cssSelector);
      is(out[i].filename, expected[i].filename, "filename matches for " + cssSelector);
      is(out[i].bubbling, expected[i].bubbling, "bubbling matches for " + cssSelector);
      is(out[i].dom0, expected[i].dom0, "dom0 matches for " + cssSelector);
      is(out[i].handler, expected[i].handler, "handlers matches for " + cssSelector);
    }

    tooltip.hide();
  }

  function promiseNextTick() {
    let deferred = promise.defer();
    executeSoon(deferred.resolve);
    return deferred.promise;
  }
});
