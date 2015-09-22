/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the cd() jsterm helper function works as expected. See bug 609872.

"use strict";

function test() {
  let hud;

  const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                   "test/test-bug-609872-cd-iframe-parent.html";

  const parentMessages = [{
    name: "document.title in parent iframe",
    text: "bug 609872 - iframe parent",
    category: CATEGORY_OUTPUT,
  }, {
    name: "paragraph content",
    text: "p: test for bug 609872 - iframe parent",
    category: CATEGORY_OUTPUT,
  }, {
    name: "object content",
    text: "obj: parent!",
    category: CATEGORY_OUTPUT,
  }];

  const childMessages = [{
    name: "document.title in child iframe",
    text: "bug 609872 - iframe child",
    category: CATEGORY_OUTPUT,
  }, {
    name: "paragraph content",
    text: "p: test for bug 609872 - iframe child",
    category: CATEGORY_OUTPUT,
  }, {
    name: "object content",
    text: "obj: child!",
    category: CATEGORY_OUTPUT,
  }];

  Task.spawn(runner).then(finishTest);

  function* runner() {
    const {tab} = yield loadTab(TEST_URI);
    hud = yield openConsole(tab);

    yield executeWindowTest();

    yield waitForMessages({ webconsole: hud, messages: parentMessages });

    info("cd() into the iframe using a selector");
    hud.jsterm.clearOutput();
    yield hud.jsterm.execute("cd('iframe')");
    yield executeWindowTest();

    yield waitForMessages({ webconsole: hud, messages: childMessages });

    info("cd() out of the iframe, reset to default window");
    hud.jsterm.clearOutput();
    yield hud.jsterm.execute("cd()");
    yield executeWindowTest();

    yield waitForMessages({ webconsole: hud, messages: parentMessages });

    info("call cd() with unexpected arguments");
    hud.jsterm.clearOutput();
    yield hud.jsterm.execute("cd(document)");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "Cannot cd()",
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_ERROR,
      }],
    });

    hud.jsterm.clearOutput();
    yield hud.jsterm.execute("cd('p')");

    yield waitForMessages({
      webconsole: hud,
      messages: [{
        text: "Cannot cd()",
        category: CATEGORY_OUTPUT,
        severity: SEVERITY_ERROR,
      }],
    });

    info("cd() into the iframe using an iframe DOM element");
    hud.jsterm.clearOutput();
    yield hud.jsterm.execute("cd($('iframe'))");
    yield executeWindowTest();

    yield waitForMessages({ webconsole: hud, messages: childMessages });

    info("cd(window.parent)");
    hud.jsterm.clearOutput();
    yield hud.jsterm.execute("cd(window.parent)");
    yield executeWindowTest();

    yield waitForMessages({ webconsole: hud, messages: parentMessages });

    yield closeConsole(tab);
  }

  function* executeWindowTest() {
    yield hud.jsterm.execute("document.title");
    yield hud.jsterm.execute("'p: ' + document.querySelector('p').textContent");
    yield hud.jsterm.execute("'obj: ' + window.foobarBug609872");
  }
}
