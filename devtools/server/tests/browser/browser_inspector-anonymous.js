/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Bug 777674

add_task(async function () {
  await SpecialPowers.pushPermissions([
    { type: "allowXULXBL", allow: true, context: MAIN_DOMAIN },
  ]);

  const { walker } = await initInspectorFront(
    MAIN_DOMAIN + "inspector-traversal-data.html"
  );

  await testXBLAnonymousInHTMLDocument(walker);
  await testNativeAnonymous(walker);
  await testNativeAnonymousStartingNode(walker);

  await testPseudoElements(walker);
  await testEmptyWithPseudo(walker);
  await testShadowAnonymous(walker);
});

async function testXBLAnonymousInHTMLDocument(walker) {
  info("Testing XBL anonymous in an HTML document.");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    const XUL_NS =
      "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    const rawToolbarbutton = content.document.createElementNS(
      XUL_NS,
      "toolbarbutton"
    );
    content.document.documentElement.appendChild(rawToolbarbutton);
  });

  const toolbarbutton = await walker.querySelector(
    walker.rootNode,
    "toolbarbutton"
  );
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

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[walker.actorID]],
    async function (actorID) {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        DevToolsServer,
      } = require("resource://devtools/server/devtools-server.js");

      const {
        DocumentWalker,
      } = require("resource://devtools/server/actors/inspector/document-walker.js");
      const nodeFilterConstants = require("resource://devtools/shared/dom-node-filter-constants.js");

      const docwalker = new DocumentWalker(
        content.document.querySelector("select"),
        content,
        {
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
      const serverWalker = DevToolsServer.searchAllConnectionsForActor(actorID);
      const node = await serverWalker.attachElement(scrollbar);

      ok(node, "A response has arrived");
      ok(node.node, "A node is in the response");
      is(
        node.node.rawNode.tagName,
        "SELECT",
        "The node has changed to a parent that the walker recognizes"
      );
    }
  );
}

async function testPseudoElements(walker) {
  info("Testing pseudo elements with walker.");

  // Markup looks like: <div><::before /><span /><::after /></div>
  const pseudo = await walker.querySelector(walker.rootNode, "#pseudo");
  const children = await walker.children(pseudo);

  is(
    pseudo.numChildren,
    1,
    "::before/::after are not counted if there is a child"
  );
  is(children.nodes.length, 3, "Correct number of children");

  const before = children.nodes[0];
  ok(before.isAnonymous, "Child is anonymous");
  ok(before._form.isNativeAnonymous, "Child is native anonymous");

  const span = children.nodes[1];
  ok(!span.isAnonymous, "Child is not anonymous");

  const after = children.nodes[2];
  ok(after.isAnonymous, "Child is anonymous");
  ok(after._form.isNativeAnonymous, "Child is native anonymous");
}

async function testEmptyWithPseudo(walker) {
  info("Testing elements with no childrent, except for pseudos.");

  info("Checking an element whose only child is a pseudo element");
  const pseudo = await walker.querySelector(walker.rootNode, "#pseudo-empty");
  const children = await walker.children(pseudo);

  is(
    pseudo.numChildren,
    1,
    "::before/::after are is counted if there are no other children"
  );
  is(children.nodes.length, 1, "Correct number of children");

  const before = children.nodes[0];
  ok(before.isAnonymous, "Child is anonymous");
  ok(before._form.isNativeAnonymous, "Child is native anonymous");
}

async function testShadowAnonymous(walker) {
  info("Testing shadow DOM content.");

  const host = await walker.querySelector(walker.rootNode, "#shadow");
  const children = await walker.children(host);

  // #shadow-root, ::before, light dom
  is(host.numChildren, 3, "Children of the shadow root are counted");
  is(children.nodes.length, 3, "Children returned from walker");

  const before = children.nodes[1];
  is(
    before._form.nodeName,
    "_moz_generated_content_before",
    "Should be the ::before pseudo-element"
  );
  ok(before.isAnonymous, "::before is anonymous");
  ok(before._form.isNativeAnonymous, "::before is native anonymous");
  info(JSON.stringify(before._form));

  const shadow = children.nodes[0];
  const shadowChildren = await walker.children(shadow);
  // <h3>...</h3>, <select multiple></select>
  is(shadow.numChildren, 2, "Children of the shadow root are counted");
  is(shadowChildren.nodes.length, 2, "Children returned from walker");

  // <h3>Shadow <em>DOM</em></h3>
  const shadowChild1 = shadowChildren.nodes[0];
  ok(!shadowChild1.isAnonymous, "Shadow child is not anonymous");
  ok(
    !shadowChild1._form.isNativeAnonymous,
    "Shadow child is not native anonymous"
  );

  const shadowSubChildren = await walker.children(shadowChild1);
  is(shadowChild1.numChildren, 2, "Subchildren of the shadow root are counted");
  is(shadowSubChildren.nodes.length, 2, "Subchildren are returned from walker");

  // <em>DOM</em>
  const shadowSubChild = shadowSubChildren.nodes[1];
  ok(
    !shadowSubChild.isAnonymous,
    "Subchildren of shadow root are not anonymous"
  );
  ok(
    !shadowSubChild._form.isNativeAnonymous,
    "Subchildren of shadow root is not native anonymous"
  );

  // <select multiple></select>
  const shadowChild2 = shadowChildren.nodes[1];
  ok(!shadowChild2.isAnonymous, "Child is anonymous");
  ok(!shadowChild2._form.isNativeAnonymous, "Child is not native anonymous");
}
