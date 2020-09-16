/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test document with no single group child
 */
addAccessibleTask(
  `<p id="p1">hello</p><p>world</p>`,
  async (browser, accDoc) => {
    let doc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    let docChildren = doc.getAttributeValue("AXChildren");
    is(docChildren.length, 1, "The document contains a root group");

    let rootGroup = docChildren[0];
    is(
      rootGroup.getAttributeValue("AXIdentifier"),
      "root-group",
      "Is generated root group"
    );

    is(
      rootGroup.getAttributeValue("AXChildren").length,
      2,
      "Root group has two children"
    );

    // From bottom-up
    let p1 = getNativeInterface(accDoc, "p1");
    rootGroup = p1.getAttributeValue("AXParent");
    is(
      rootGroup.getAttributeValue("AXIdentifier"),
      "root-group",
      "Is generated root group"
    );
  }
);

/**
 * Test document with a top-level group
 */
addAccessibleTask(
  `<div role="grouping" id="group"><p>hello</p><p>world</p></div>`,
  async (browser, accDoc) => {
    let doc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    let docChildren = doc.getAttributeValue("AXChildren");
    is(docChildren.length, 1, "The document contains a root group");

    let rootGroup = docChildren[0];
    is(
      rootGroup.getAttributeValue("AXDOMIdentifier"),
      "group",
      "Root group is a document element"
    );
  }
);

/**
 * Test document with a single button
 */
addAccessibleTask(
  `<button id="button">I am a button</button>`,
  async (browser, accDoc) => {
    let doc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );
    let docChildren = doc.getAttributeValue("AXChildren");
    is(docChildren.length, 1, "The document contains a root group");

    let rootGroup = docChildren[0];
    is(
      rootGroup.getAttributeValue("AXIdentifier"),
      "root-group",
      "Is generated root group"
    );

    let rootGroupChildren = rootGroup.getAttributeValue("AXChildren");
    is(rootGroupChildren.length, 1, "Root group has one children");

    is(
      rootGroupChildren[0].getAttributeValue("AXRole"),
      "AXButton",
      "Button is child of root group"
    );

    // From bottom-up
    let button = getNativeInterface(accDoc, "button");
    rootGroup = button.getAttributeValue("AXParent");
    is(
      rootGroup.getAttributeValue("AXIdentifier"),
      "root-group",
      "Is generated root group"
    );
  }
);
