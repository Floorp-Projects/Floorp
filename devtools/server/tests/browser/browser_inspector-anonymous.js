/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Bug 777674

add_task(async function() {
  const { walker } =
    await initInspectorFront(MAIN_DOMAIN + "inspector-traversal-data.html");

  await testXBLAnonymousInHTMLDocument(walker);
  await testNativeAnonymous(walker);
  await testNativeAnonymousStartingNode(walker);

  await testPseudoElements(walker);
  await testEmptyWithPseudo(walker);
  await testShadowAnonymous(walker);
});

async function testXBLAnonymousInHTMLDocument(walker) {
  info("Testing XBL anonymous in an HTML document.");
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    const rawToolbarbutton = content.document.createElementNS(XUL_NS, "toolbarbutton");
    content.document.documentElement.appendChild(rawToolbarbutton);
  });

  const toolbarbutton = await walker.querySelector(walker.rootNode, "toolbarbutton");
  const children = await walker.children(toolbarbutton);

  is(toolbarbutton.numChildren, 0, "XBL content is not visible in HTML doc");
  is(children.nodes.length, 0, "XBL content is not returned in HTML doc");
}

async function testNativeAnonymous(walker) {
  info("Testing native anonymous content with walker.");

  const select = await walker.querySelector(walker.rootNode, "select");
  const children = await walker.children(select);

  is(select.numChildren, 2, "No native anon content for form control");
  is(children.nodes.length, 2, "No native anon content for form control");
}

async function testNativeAnonymousStartingNode(walker) {
  info("Tests attaching an element that a walker can't see.");

  await ContentTask.spawn(gBrowser.selectedBrowser, [walker.actorID],
    async function(actorID) {
      const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
      const { DebuggerServer } = require("devtools/server/main");

      const {DocumentWalker} =
        require("devtools/server/actors/inspector/document-walker");
      const nodeFilterConstants =
        require("devtools/shared/dom-node-filter-constants");

      const docwalker = new DocumentWalker(
        content.document.querySelector("select"),
        content,
        {
          whatToShow: nodeFilterConstants.SHOW_ALL,
          filter: () => {
            return nodeFilterConstants.FILTER_ACCEPT;
          },
        }
      );
      const scrollbar = docwalker.lastChild();
      is(scrollbar.tagName, "scrollbar", "An anonymous child has been fetched");

      // Convert actorID to current compartment string otherwise
      // searchAllConnectionsForActor is confused and won't find the actor.
      actorID = String(actorID);
      const serverWalker = DebuggerServer.searchAllConnectionsForActor(actorID);
      const node = await serverWalker.attachElement(scrollbar);

      ok(node, "A response has arrived");
      ok(node.node, "A node is in the response");
      is(node.node.rawNode.tagName, "SELECT",
        "The node has changed to a parent that the walker recognizes");
    });
}

async function testPseudoElements(walker) {
  info("Testing pseudo elements with walker.");

  // Markup looks like: <div><::before /><span /><::after /></div>
  const pseudo = await walker.querySelector(walker.rootNode, "#pseudo");
  const children = await walker.children(pseudo);

  is(pseudo.numChildren, 1, "::before/::after are not counted if there is a child");
  is(children.nodes.length, 3, "Correct number of children");

  const before = children.nodes[0];
  ok(before.isAnonymous, "Child is anonymous");
  ok(!before._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(!before._form.isShadowAnonymous, "Child is not shadow anonymous");
  ok(before._form.isNativeAnonymous, "Child is native anonymous");

  const span = children.nodes[1];
  ok(!span.isAnonymous, "Child is not anonymous");

  const after = children.nodes[2];
  ok(after.isAnonymous, "Child is anonymous");
  ok(!after._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(!after._form.isShadowAnonymous, "Child is not shadow anonymous");
  ok(after._form.isNativeAnonymous, "Child is native anonymous");
}

async function testEmptyWithPseudo(walker) {
  info("Testing elements with no childrent, except for pseudos.");

  info("Checking an element whose only child is a pseudo element");
  const pseudo = await walker.querySelector(walker.rootNode, "#pseudo-empty");
  const children = await walker.children(pseudo);

  is(pseudo.numChildren, 1,
     "::before/::after are is counted if there are no other children");
  is(children.nodes.length, 1, "Correct number of children");

  const before = children.nodes[0];
  ok(before.isAnonymous, "Child is anonymous");
  ok(!before._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(!before._form.isShadowAnonymous, "Child is not shadow anonymous");
  ok(before._form.isNativeAnonymous, "Child is native anonymous");
}

async function testShadowAnonymous(walker) {
  if (true) {
    // FIXME(bug 1465114)
    return;
  }

  info("Testing shadow DOM content.");

  const shadow = await walker.querySelector(walker.rootNode, "#shadow");
  const children = await walker.children(shadow);

  is(shadow.numChildren, 3, "Children of the shadow root are counted");
  is(children.nodes.length, 3, "Children returned from walker");

  const before = children.nodes[0];
  ok(before.isAnonymous, "Child is anonymous");
  ok(!before._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(!before._form.isShadowAnonymous, "Child is not shadow anonymous");
  ok(before._form.isNativeAnonymous, "Child is native anonymous");

  // <h3>Shadow <em>DOM</em></h3>
  const shadowChild1 = children.nodes[1];
  ok(shadowChild1.isAnonymous, "Child is anonymous");
  ok(!shadowChild1._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(shadowChild1._form.isShadowAnonymous, "Child is shadow anonymous");
  ok(!shadowChild1._form.isNativeAnonymous, "Child is not native anonymous");

  const shadowSubChildren = await walker.children(children.nodes[1]);
  is(shadowChild1.numChildren, 2, "Subchildren of the shadow root are counted");
  is(shadowSubChildren.nodes.length, 2, "Subchildren are returned from walker");

  // <em>DOM</em>
  const shadowSubChild = children.nodes[1];
  ok(shadowSubChild.isAnonymous, "Child is anonymous");
  ok(!shadowSubChild._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(shadowSubChild._form.isShadowAnonymous, "Child is shadow anonymous");
  ok(!shadowSubChild._form.isNativeAnonymous, "Child is not native anonymous");

  // <select multiple></select>
  const shadowChild2 = children.nodes[2];
  ok(shadowChild2.isAnonymous, "Child is anonymous");
  ok(!shadowChild2._form.isXBLAnonymous, "Child is not XBL anonymous");
  ok(shadowChild2._form.isShadowAnonymous, "Child is shadow anonymous");
  ok(!shadowChild2._form.isNativeAnonymous, "Child is not native anonymous");
}
