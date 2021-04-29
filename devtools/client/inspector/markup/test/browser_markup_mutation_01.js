/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that various mutations to the dom update the markup view correctly.

const TEST_URL = URL_ROOT + "doc_markup_mutation.html";

// Mutation tests. Each entry in the array has the following properties:
// - desc: for logging only
// - numMutations: how many mutations are expected to come happen due to the
//   test case.  Defaults to 1 if not set.
// - test: a function supposed to mutate the DOM
// - check: a function supposed to test that the mutation was handled
const TEST_DATA = [
  {
    desc: "Adding an attribute",
    test: async function(testActor) {
      const browser = gBrowser.selectedBrowser;
      await setAttributeInBrowser(browser, "#node1", "newattr", "newattrval");
    },
    check: async function(inspector) {
      const { editor } = await getContainerForSelector("#node1", inspector);
      ok(
        [...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
          return (
            attr.textContent.trim() === 'newattr="newattrval"' &&
            attr.dataset.value === "newattrval" &&
            attr.dataset.attr === "newattr"
          );
        }),
        "newattr attribute found"
      );
    },
  },
  {
    desc: "Removing an attribute",
    test: async function(testActor) {
      const browser = gBrowser.selectedBrowser;
      await removeAttributeInBrowser(browser, "#node1", "newattr");
    },
    check: async function(inspector) {
      const { editor } = await getContainerForSelector("#node1", inspector);
      ok(
        ![...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
          return attr.textContent.trim() === 'newattr="newattrval"';
        }),
        "newattr attribute removed"
      );
    },
  },
  {
    desc: "Re-adding an attribute",
    test: async function(testActor) {
      const browser = gBrowser.selectedBrowser;
      await setAttributeInBrowser(browser, "#node1", "newattr", "newattrval");
    },
    check: async function(inspector) {
      const { editor } = await getContainerForSelector("#node1", inspector);
      ok(
        [...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
          return (
            attr.textContent.trim() === 'newattr="newattrval"' &&
            attr.dataset.value === "newattrval" &&
            attr.dataset.attr === "newattr"
          );
        }),
        "newattr attribute found"
      );
    },
  },
  {
    desc: "Changing an attribute",
    test: async function() {
      const browser = gBrowser.selectedBrowser;
      await setAttributeInBrowser(
        browser,
        "#node1",
        "newattr",
        "newattrchanged"
      );
    },
    check: async function(inspector) {
      const { editor } = await getContainerForSelector("#node1", inspector);
      ok(
        [...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
          return (
            attr.textContent.trim() === 'newattr="newattrchanged"' &&
            attr.dataset.value === "newattrchanged" &&
            attr.dataset.attr === "newattr"
          );
        }),
        "newattr attribute found"
      );
    },
  },
  {
    desc: "Adding another attribute does not rerender unchanged attributes",
    test: async function(testActor, inspector) {
      const { editor } = await getContainerForSelector("#node1", inspector);

      // This test checks the impact on the markup-view nodes after setting attributes on
      // content nodes.
      info("Expect attribute-container for 'new-attr' from the previous test");
      const attributeContainer = editor.attrList.querySelector(
        "[data-attr=newattr]"
      );
      ok(attributeContainer, "attribute-container for 'newattr' found");

      info("Set a flag on the attribute-container to check after the mutation");
      attributeContainer.beforeMutationFlag = true;

      info(
        "Add the attribute 'otherattr' on the content node to trigger the mutation"
      );
      const browser = gBrowser.selectedBrowser;
      await setAttributeInBrowser(browser, "#node1", "otherattr", "othervalue");
    },
    check: async function(inspector) {
      const { editor } = await getContainerForSelector("#node1", inspector);

      info(
        "Check the attribute-container for the new attribute mutation was created"
      );
      const otherAttrContainer = editor.attrList.querySelector(
        "[data-attr=otherattr]"
      );
      ok(otherAttrContainer, "attribute-container for 'otherattr' found");

      info(
        "Check the attribute-container for 'new-attr' is the same node as earlier."
      );
      const newAttrContainer = editor.attrList.querySelector(
        "[data-attr=newattr]"
      );
      ok(newAttrContainer, "attribute-container for 'newattr' found");
      ok(
        newAttrContainer.beforeMutationFlag,
        "attribute-container same as earlier"
      );
    },
  },
  {
    desc: "Adding ::after element",
    numMutations: 2,
    test: async function(testActor) {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node1 = content.document.querySelector("#node1");
        node1.classList.add("pseudo");
      });
    },
    check: async function(inspector) {
      const { children } = await getContainerForSelector("#node1", inspector);
      is(
        children.childNodes.length,
        2,
        "Node1 now has 2 children (text child and ::after"
      );
    },
  },
  {
    desc: "Removing ::after element",
    numMutations: 2,
    test: async function(testActor) {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node1 = content.document.querySelector("#node1");
        node1.classList.remove("pseudo");
      });
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node1", inspector);
      ok(container.inlineTextChild, "Has single text child.");
    },
  },
  {
    desc: "Updating the text-content",
    test: async function(testActor) {
      await testActor.setProperty("#node1", "textContent", "newtext");
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node1", inspector);
      ok(container.inlineTextChild, "Has single text child.");
      ok(!container.canExpand, "Can't expand container with inlineTextChild.");
      ok(!container.inlineTextChild.canExpand, "Can't expand inlineTextChild.");
      is(
        container.editor.elt.querySelector(".text").textContent.trim(),
        "newtext",
        "Single text child editor updated."
      );
    },
  },
  {
    desc: "Adding a second text child",
    test: async function(testActor) {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node1 = content.document.querySelector("#node1");
        const newText = node1.ownerDocument.createTextNode("more");
        node1.appendChild(newText);
      });
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node1", inspector);
      ok(!container.inlineTextChild, "Does not have single text child.");
      ok(container.canExpand, "Can expand container with child nodes.");
      ok(
        container.editor.elt.querySelector(".text") == null,
        "Single text child editor removed."
      );
    },
  },
  {
    desc: "Go from 2 to 1 text child",
    test: async function(testActor) {
      await testActor.setProperty("#node1", "textContent", "newtext");
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node1", inspector);
      ok(container.inlineTextChild, "Has single text child.");
      ok(!container.canExpand, "Can't expand container with inlineTextChild.");
      ok(!container.inlineTextChild.canExpand, "Can't expand inlineTextChild.");
      is(
        container.editor.elt.querySelector(".text").textContent.trim(),
        "newtext",
        "Single text child editor updated."
      );
    },
  },
  {
    desc: "Removing an only text child",
    test: async function(testActor) {
      await testActor.setProperty("#node1", "innerHTML", "");
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node1", inspector);
      ok(!container.inlineTextChild, "Does not have single text child.");
      ok(!container.canExpand, "Can't expand empty container.");
      ok(
        container.editor.elt.querySelector(".text") == null,
        "Single text child editor removed."
      );
    },
  },
  {
    desc: "Go from 0 to 1 text child",
    test: async function(testActor) {
      await testActor.setProperty("#node1", "textContent", "newtext");
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node1", inspector);
      ok(container.inlineTextChild, "Has single text child.");
      ok(!container.canExpand, "Can't expand container with inlineTextChild.");
      ok(!container.inlineTextChild.canExpand, "Can't expand inlineTextChild.");
      is(
        container.editor.elt.querySelector(".text").textContent.trim(),
        "newtext",
        "Single text child editor updated."
      );
    },
  },

  {
    desc: "Updating the innerHTML",
    test: async function(testActor) {
      await testActor.setProperty(
        "#node2",
        "innerHTML",
        "<div><span>foo</span></div>"
      );
    },
    check: async function(inspector) {
      const container = await getContainerForSelector("#node2", inspector);

      const openTags = container.children.querySelectorAll(".open .tag");
      is(openTags.length, 2, "There are 2 tags in node2");
      is(openTags[0].textContent.trim(), "div", "The first tag is a div");
      is(openTags[1].textContent.trim(), "span", "The second tag is a span");

      is(
        container.children.querySelector(".text").textContent.trim(),
        "foo",
        "The span's textcontent is correct"
      );
    },
  },
  {
    desc: "Removing child nodes",
    test: async function(testActor) {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node4 = content.document.querySelector("#node4");
        while (node4.firstChild) {
          node4.firstChild.remove();
        }
      });
    },
    check: async function(inspector) {
      const { children } = await getContainerForSelector("#node4", inspector);
      is(children.innerHTML, "", "Children have been removed");
    },
  },
  {
    desc: "Appending a child to a different parent",
    test: async function(testActor) {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node17 = content.document.querySelector("#node17");
        const node2 = content.document.querySelector("#node2");
        node2.appendChild(node17);
      });
    },
    check: async function(inspector) {
      const { children } = await getContainerForSelector("#node16", inspector);
      is(
        children.innerHTML,
        "",
        "Node17 has been removed from its node16 parent"
      );

      const container = await getContainerForSelector("#node2", inspector);
      const openTags = container.children.querySelectorAll(".open .tag");
      is(openTags.length, 3, "There are now 3 tags in node2");
      is(openTags[2].textContent.trim(), "p", "The third tag is node17");
    },
  },
  {
    desc: "Swapping a parent and child element, putting them in the same tree",
    // body
    //  node1
    //  node18
    //    node19
    //      node20
    //        node21
    // will become:
    // body
    //   node1
    //     node20
    //      node21
    //      node18
    //        node19
    test: async function(testActor) {
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        const node18 = content.document.querySelector("#node18");
        const node20 = content.document.querySelector("#node20");
        const node1 = content.document.querySelector("#node1");
        node1.appendChild(node20);
        node20.appendChild(node18);
      });
    },
    check: async function(inspector) {
      await inspector.markup.expandAll();

      const { children } = await getContainerForSelector("#node1", inspector);
      is(
        children.childNodes.length,
        2,
        "Node1 now has 2 children (textnode and node20)"
      );

      const node20 = children.childNodes[1];
      const node20Children = node20.container.children;
      is(
        node20Children.childNodes.length,
        2,
        "Node20 has 2 children (21 and 18)"
      );

      const node21 = node20Children.childNodes[0];
      is(
        node21.container.editor.elt.querySelector(".text").textContent.trim(),
        "line21",
        "Node21 has a single text child"
      );

      const node18 = node20Children.childNodes[1];
      is(
        node18
          .querySelector(".open .attreditor .attr-value")
          .textContent.trim(),
        "node18",
        "Node20's second child is indeed node18"
      );
    },
  },
];

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);

  info("Expanding all markup-view nodes");
  await inspector.markup.expandAll();

  for (let { desc, test, check, numMutations } of TEST_DATA) {
    info("Starting test: " + desc);

    numMutations = numMutations || 1;

    info("Executing the test markup mutation");

    // If a test expects more than one mutation it may come through in a single
    // event or possibly in multiples.
    let seenMutations = 0;
    const promise = new Promise(resolve => {
      inspector.on("markupmutation", function onmutation(mutations) {
        seenMutations += mutations.length;
        info(
          "Receieved " +
            seenMutations +
            " mutations, expecting at least " +
            numMutations
        );
        if (seenMutations >= numMutations) {
          inspector.off("markupmutation", onmutation);
          resolve();
        }
      });
    });
    await test(testActor, inspector);
    await promise;

    info("Expanding all markup-view nodes to make sure new nodes are imported");
    await inspector.markup.expandAll();

    info("Checking the markup-view content");
    await check(inspector);
  }
});
