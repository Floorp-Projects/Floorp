/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that various mutations to the dom update the markup view correctly.

const TEST_URL = TEST_URL_ROOT + "doc_markup_mutation.html";

// Mutation tests. Each entry in the array has the following properties:
// - desc: for logging only
// - numMutations: how many mutations are expected to come happen due to the
//   test case.  Defaults to 1 if not set.
// - test: a function supposed to mutate the DOM
// - check: a function supposed to test that the mutation was handled
const TEST_DATA = [
  {
    desc: "Adding an attribute",
    test: () => {
      let node1 = getNode("#node1");
      node1.setAttribute("newattr", "newattrval");
    },
    check: function*(inspector) {
      let {editor} = yield getContainerForSelector("#node1", inspector);
      ok([...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
        return attr.textContent.trim() === "newattr=\"newattrval\"";
      }), "newattr attribute found");
    }
  },
  {
    desc: "Removing an attribute",
    test: () => {
      let node1 = getNode("#node1");
      node1.removeAttribute("newattr");
    },
    check: function*(inspector) {
      let {editor} = yield getContainerForSelector("#node1", inspector);
      ok(![...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
        return attr.textContent.trim() === "newattr=\"newattrval\"";
      }), "newattr attribute removed");
    }
  },
  {
    desc: "Re-adding an attribute",
    test: () => {
      let node1 = getNode("#node1");
      node1.setAttribute("newattr", "newattrval");
    },
    check: function*(inspector) {
      let {editor} = yield getContainerForSelector("#node1", inspector);
      ok([...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
        return attr.textContent.trim() === "newattr=\"newattrval\"";
      }), "newattr attribute found");
    }
  },
  {
    desc: "Changing an attribute",
    test: () => {
      let node1 = getNode("#node1");
      node1.setAttribute("newattr", "newattrchanged");
    },
    check: function*(inspector) {
      let {editor} = yield getContainerForSelector("#node1", inspector);
      ok([...editor.attrList.querySelectorAll(".attreditor")].some(attr => {
        return attr.textContent.trim() === "newattr=\"newattrchanged\"";
      }), "newattr attribute found");
    }
  },
  {
    desc: "Adding ::after element",
    numMutations: 2,
    test: () => {
      let node1 = getNode("#node1");
      node1.classList.add("pseudo");
    },
    check: function*(inspector) {
      let {children} = yield getContainerForSelector("#node1", inspector);
      is(children.childNodes.length, 2,
        "Node1 now has 2 children (text child and ::after");
    }
  },
  {
    desc: "Removing ::after element",
    numMutations: 2,
    test: () => {
      let node1 = getNode("#node1");
      node1.classList.remove("pseudo");
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node1", inspector);
      ok(container.singleTextChild, "Has single text child.");
    }
  },
  {
    desc: "Updating the text-content",
    test: () => {
      let node1 = getNode("#node1");
      node1.textContent = "newtext";
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node1", inspector);
      ok(container.singleTextChild, "Has single text child.");
      ok(!container.canExpand, "Can't expand container with singleTextChild.");
      ok(!container.singleTextChild.canExpand, "Can't expand singleTextChild.");
      is(container.editor.elt.querySelector(".text").textContent.trim(), "newtext",
        "Single text child editor updated.");
    }
  },
  {
    desc: "Adding a second text child",
    test: () => {
      let node1 = getNode("#node1");
      let newText = node1.ownerDocument.createTextNode("more");
      node1.appendChild(newText);
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node1", inspector);
      ok(!container.singleTextChild, "Does not have single text child.");
      ok(container.canExpand, "Can expand container with child nodes.");
      ok(container.editor.elt.querySelector(".text") == null,
        "Single text child editor removed.");
    },
  },
  {
    desc: "Go from 2 to 1 text child",
    test: () => {
      let node1 = getNode("#node1");
      node1.textContent = "newtext";
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node1", inspector);
      ok(container.singleTextChild, "Has single text child.");
      ok(!container.canExpand, "Can't expand container with singleTextChild.");
      ok(!container.singleTextChild.canExpand, "Can't expand singleTextChild.");
      ok(container.editor.elt.querySelector(".text").textContent.trim(), "newtext",
        "Single text child editor updated.");
    },
  },
  {
    desc: "Removing an only text child",
    test: () => {
      let node1 = getNode("#node1");
      node1.innerHTML = "";
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node1", inspector);
      ok(!container.singleTextChild, "Does not have single text child.");
      ok(!container.canExpand, "Can't expand empty container.");
      ok(container.editor.elt.querySelector(".text") == null,
        "Single text child editor removed.");
    },
  },
  {
    desc: "Go from 0 to 1 text child",
    test: () => {
      let node1 = getNode("#node1");
      node1.textContent = "newtext";
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node1", inspector);
      ok(container.singleTextChild, "Has single text child.");
      ok(!container.canExpand, "Can't expand container with singleTextChild.");
      ok(!container.singleTextChild.canExpand, "Can't expand singleTextChild.");
      ok(container.editor.elt.querySelector(".text").textContent.trim(), "newtext",
        "Single text child editor updated.");
    },
  },

  {
    desc: "Updating the innerHTML",
    test: () => {
      let node2 = getNode("#node2");
      node2.innerHTML = "<div><span>foo</span></div>";
    },
    check: function*(inspector) {
      let container = yield getContainerForSelector("#node2", inspector);

      let openTags = container.children.querySelectorAll(".open .tag");
      is(openTags.length, 2, "There are 2 tags in node2");
      is(openTags[0].textContent.trim(), "div", "The first tag is a div");
      is(openTags[1].textContent.trim(), "span", "The second tag is a span");

      is(container.children.querySelector(".text").textContent.trim(), "foo",
        "The span's textcontent is correct");
    }
  },
  {
    desc: "Removing child nodes",
    test: () => {
      let node4 = getNode("#node4");
      while (node4.firstChild) {
        node4.removeChild(node4.firstChild);
      }
    },
    check: function*(inspector) {
      let {children} = yield getContainerForSelector("#node4", inspector);
      is(children.innerHTML, "", "Children have been removed");
    }
  },
  {
    desc: "Appending a child to a different parent",
    test: () => {
      let node17 = getNode("#node17");
      let node2 = getNode("#node2");
      node2.appendChild(node17);
    },
    check: function*(inspector) {
      let {children} = yield getContainerForSelector("#node16", inspector);
      is(children.innerHTML, "", "Node17 has been removed from its node16 parent");

      let container = yield getContainerForSelector("#node2", inspector);
      let openTags = container.children.querySelectorAll(".open .tag");
      is(openTags.length, 3, "There are now 3 tags in node2");
      is(openTags[2].textContent.trim(), "p", "The third tag is node17");
    }
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
    test: () => {
      let node18 = getNode("#node18");
      let node20 = getNode("#node20");

      let node1 = getNode("#node1");

      node1.appendChild(node20);
      node20.appendChild(node18);
    },
    check: function*(inspector) {
      yield inspector.markup.expandAll();

      let {children} = yield getContainerForSelector("#node1", inspector);
      is(children.childNodes.length, 2,
        "Node1 now has 2 children (textnode and node20)");

      let node20 = children.childNodes[1];
      let node20Children = node20.container.children;
      is(node20Children.childNodes.length, 2,
          "Node20 has 2 children (21 and 18)");

      let node21 = node20Children.childNodes[0];
      is(node21.container.editor.elt.querySelector(".text").textContent.trim(), "line21",
        "Node21 has a single text child");

      let node18 = node20Children.childNodes[1];
      is(node18.querySelector(".open .attreditor .attr-value").textContent.trim(),
        "node18", "Node20's second child is indeed node18");
    }
  }
];

add_task(function*() {
  let {toolbox, inspector} = yield addTab(TEST_URL).then(openInspector);

  info("Expanding all markup-view nodes");
  yield inspector.markup.expandAll();

  for (let {desc, test, check, numMutations} of TEST_DATA) {
    info("Starting test: " + desc);

    numMutations = numMutations || 1;

    info("Executing the test markup mutation");
    yield new Promise((resolve) => {
      // If a test expects more than one mutation it may come through in a
      // single event or possibly in multiples.
      let seenMutations = 0;
      inspector.on("markupmutation", function onmutation(e, mutations) {
        seenMutations += mutations.length;
        info("Receieved " + seenMutations +
             " mutations, expecting at least " + numMutations);
        if (seenMutations >= numMutations) {
          inspector.off("markupmutation", onmutation);
          resolve();
        }
      });

      test();
    })

    info("Expanding all markup-view nodes to make sure new nodes are imported");
    yield inspector.markup.expandAll();

    info("Checking the markup-view content");
    yield check(inspector);
  }
});
