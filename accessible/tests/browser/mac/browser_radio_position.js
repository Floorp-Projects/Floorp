/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

function getChildRoles(parent) {
  return parent
    .getAttributeValue("AXChildren")
    .map(c => c.getAttributeValue("AXRole"));
}

function getLinkedTitles(element) {
  return element
    .getAttributeValue("AXLinkedUIElements")
    .map(c => c.getAttributeValue("AXTitle"));
}

/**
 * Test radio group
 */
addAccessibleTask(
  `<div role="radiogroup" id="radioGroup">
    <div role="radio"
         id="radioGroupItem1">
       Regular crust
    </div>
    <div role="radio"
         id="radioGroupItem2">
       Deep dish
    </div>
    <div role="radio"
         id="radioGroupItem3">
       Thin crust
    </div>
  </div>`,
  async (browser, accDoc) => {
    let item1 = getNativeInterface(accDoc, "radioGroupItem1");
    let item2 = getNativeInterface(accDoc, "radioGroupItem2");
    let item3 = getNativeInterface(accDoc, "radioGroupItem3");
    let titleList = ["Regular crust", "Deep dish", "Thin crust"];

    Assert.deepEqual(
      titleList,
      [item1, item2, item3].map(c => c.getAttributeValue("AXTitle")),
      "Title list matches"
    );

    let linkedElems = item1.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Item 1 has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(item1),
      titleList,
      "Item one has correctly ordered linked elements"
    );

    linkedElems = item2.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Item 2 has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(item2),
      titleList,
      "Item two has correctly ordered linked elements"
    );

    linkedElems = item3.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Item 3 has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(item3),
      titleList,
      "Item three has correctly ordered linked elements"
    );
  }
);

/**
 * Test dynamic add to a radio group
 */
addAccessibleTask(
  `<div role="radiogroup" id="radioGroup">
    <div role="radio"
         id="radioGroupItem1">
       Option One
    </div>
  </div>`,
  async (browser, accDoc) => {
    let item1 = getNativeInterface(accDoc, "radioGroupItem1");
    let linkedElems = item1.getAttributeValue("AXLinkedUIElements");

    is(linkedElems.length, 1, "Item 1 has one linked UI elem");
    is(
      linkedElems[0].getAttributeValue("AXTitle"),
      item1.getAttributeValue("AXTitle"),
      "Item 1 is first element"
    );

    let reorder = waitForEvent(EVENT_REORDER, "radioGroup");
    await SpecialPowers.spawn(browser, [], () => {
      let d = content.document.createElement("div");
      d.setAttribute("role", "radio");
      content.document.getElementById("radioGroup").appendChild(d);
    });
    await reorder;

    let radioGroup = getNativeInterface(accDoc, "radioGroup");
    let groupMembers = radioGroup.getAttributeValue("AXChildren");
    is(groupMembers.length, 2, "Radio group has two members");
    let item2 = groupMembers[1];
    item1 = getNativeInterface(accDoc, "radioGroupItem1");
    let titleList = ["Option One", ""];

    Assert.deepEqual(
      titleList,
      [item1, item2].map(c => c.getAttributeValue("AXTitle")),
      "Title list matches"
    );

    linkedElems = item1.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 2, "Item 1 has two linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(item1),
      titleList,
      "Item one has correctly ordered linked elements"
    );

    linkedElems = item2.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 2, "Item 2 has two linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(item2),
      titleList,
      "Item two has correctly ordered linked elements"
    );
  }
);

/**
 * Test input[type=radio] for single group
 */
addAccessibleTask(
  `<input type="radio" id="cat" name="animal"><label for="cat">Cat</label>
   <input type="radio" id="dog" name="animal"><label for="dog">Dog</label>
   <input type="radio" id="catdog" name="animal"><label for="catdog">CatDog</label>`,
  async (browser, accDoc) => {
    let cat = getNativeInterface(accDoc, "cat");
    let dog = getNativeInterface(accDoc, "dog");
    let catdog = getNativeInterface(accDoc, "catdog");
    let titleList = ["Cat", "Dog", "CatDog"];

    Assert.deepEqual(
      titleList,
      [cat, dog, catdog].map(x => x.getAttributeValue("AXTitle")),
      "Title list matches"
    );

    let linkedElems = cat.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Cat has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(cat),
      titleList,
      "Cat has correctly ordered linked elements"
    );

    linkedElems = dog.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Dog has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(dog),
      titleList,
      "Dog has correctly ordered linked elements"
    );

    linkedElems = catdog.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Catdog has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(catdog),
      titleList,
      "catdog has correctly ordered linked elements"
    );
  }
);

/**
 * Test input[type=radio] for different groups
 */
addAccessibleTask(
  `<input type="radio" id="cat" name="one"><label for="cat">Cat</label>
   <input type="radio" id="dog" name="two"><label for="dog">Dog</label>
   <input type="radio" id="catdog"><label for="catdog">CatDog</label>`,
  async (browser, accDoc) => {
    let cat = getNativeInterface(accDoc, "cat");
    let dog = getNativeInterface(accDoc, "dog");
    let catdog = getNativeInterface(accDoc, "catdog");

    let linkedElems = cat.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 1, "Cat has one linked UI elem");
    is(
      linkedElems[0].getAttributeValue("AXTitle"),
      cat.getAttributeValue("AXTitle"),
      "Cat is only element"
    );

    linkedElems = dog.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 1, "Dog has one linked UI elem");
    is(
      linkedElems[0].getAttributeValue("AXTitle"),
      dog.getAttributeValue("AXTitle"),
      "Dog is only element"
    );

    linkedElems = catdog.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 0, "Catdog has no linked UI elem");
  }
);

/**
 * Test input[type=radio] for single group across DOM
 */
addAccessibleTask(
  `<input type="radio" id="cat" name="animal"><label for="cat">Cat</label>
   <div>
    <span>
      <input type="radio" id="dog" name="animal"><label for="dog">Dog</label>
    </span>
   </div>
   <div>
   <input type="radio" id="catdog" name="animal"><label for="catdog">CatDog</label>
   </div>`,
  async (browser, accDoc) => {
    let cat = getNativeInterface(accDoc, "cat");
    let dog = getNativeInterface(accDoc, "dog");
    let catdog = getNativeInterface(accDoc, "catdog");
    let titleList = ["Cat", "Dog", "CatDog"];

    Assert.deepEqual(
      titleList,
      [cat, dog, catdog].map(x => x.getAttributeValue("AXTitle")),
      "Title list matches"
    );

    let linkedElems = cat.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Cat has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(cat),
      titleList,
      "cat has correctly ordered linked elements"
    );

    linkedElems = dog.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Dog has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(dog),
      titleList,
      "dog has correctly ordered linked elements"
    );

    linkedElems = catdog.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 3, "Catdog has three linked UI elems");
    Assert.deepEqual(
      getLinkedTitles(catdog),
      titleList,
      "catdog has correctly ordered linked elements"
    );
  }
);

/**
 * Test dynamic add of input[type=radio] in a single group
 */
addAccessibleTask(
  `<div id="container"><input type="radio" id="cat" name="animal"></div>`,
  async (browser, accDoc) => {
    let cat = getNativeInterface(accDoc, "cat");
    let container = getNativeInterface(accDoc, "container");

    let containerChildren = container.getAttributeValue("AXChildren");
    is(containerChildren.length, 1, "container has one button");
    is(
      containerChildren[0].getAttributeValue("AXRole"),
      "AXRadioButton",
      "Container child is radio button"
    );

    let linkedElems = cat.getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 1, "Cat has 1 linked UI elem");
    is(
      linkedElems[0].getAttributeValue("AXTitle"),
      cat.getAttributeValue("AXTitle"),
      "Cat is first element"
    );
    let reorder = waitForEvent(EVENT_REORDER, "container");
    await SpecialPowers.spawn(browser, [], () => {
      let input = content.document.createElement("input");
      input.setAttribute("type", "radio");
      input.setAttribute("name", "animal");
      content.document.getElementById("container").appendChild(input);
    });
    await reorder;

    container = getNativeInterface(accDoc, "container");
    containerChildren = container.getAttributeValue("AXChildren");

    is(containerChildren.length, 2, "container has two children");

    Assert.deepEqual(
      getChildRoles(container),
      ["AXRadioButton", "AXRadioButton"],
      "Both children are radio buttons"
    );

    linkedElems = containerChildren[0].getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 2, "Cat has 2 linked elements");

    linkedElems = containerChildren[1].getAttributeValue("AXLinkedUIElements");
    is(linkedElems.length, 2, "New button has 2 linked elements");
  }
);
