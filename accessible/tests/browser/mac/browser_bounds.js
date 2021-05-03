/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test position, size for onscreen content
 */
addAccessibleTask(
  `I am some extra content<br>
   <div id="hello" style="display:inline;">hello</div><br>
   <div id="world" style="display:inline;">hello world<br>I am some text</div>`,
  async (browser, accDoc) => {
    const hello = getNativeInterface(accDoc, "hello");
    const world = getNativeInterface(accDoc, "world");
    ok(hello.getAttributeValue("AXFrame"), "Hello's frame attr is not null");
    ok(world.getAttributeValue("AXFrame"), "World's frame attr is not null");

    // AXSize and AXPosition are composed of AXFrame components, so we
    // test them here instead of calling AXFrame directly.
    const [helloWidth, helloHeight] = hello.getAttributeValue("AXSize");
    const [worldWidth, worldHeight] = world.getAttributeValue("AXSize");
    ok(helloWidth > 0, "Hello has a positive width");
    ok(helloHeight > 0, "Hello has a positive height");
    ok(worldWidth > 0, "World has a positive width");
    ok(worldHeight > 0, "World has a positive height");
    ok(helloHeight < worldHeight, "Hello has a smaller height than world");
    ok(helloWidth < worldWidth, "Hello has a smaller width than world");

    // Note: these are mac screen coords, so our origin is bottom left
    const [helloX, helloY] = hello.getAttributeValue("AXPosition");
    const [worldX, worldY] = world.getAttributeValue("AXPosition");
    ok(helloX > 0, "Hello has a positive X");
    ok(helloY > 0, "Hello has a positive Y");
    ok(worldX > 0, "World has a positive X");
    ok(worldY > 0, "World has a positive Y");
    ok(helloY > worldY, "Hello has a larger Y than world");
    ok(helloX == worldX, "Hello and world have the same X");
  }
);

/**
 * Test position, size for offscreen content
 */
addAccessibleTask(
  `I am some extra content<br>
   <div id="hello" style="display:inline; position:absolute; left:-2000px;">hello</div><br>
   <div id="world" style="display:inline; position:absolute; left:-2000px;">hello world<br>I am some text</div>`,
  async (browser, accDoc) => {
    const hello = getNativeInterface(accDoc, "hello");
    const world = getNativeInterface(accDoc, "world");
    ok(hello.getAttributeValue("AXFrame"), "Hello's frame attr is not null");
    ok(world.getAttributeValue("AXFrame"), "World's frame attr is not null");

    // AXSize and AXPosition are composed of AXFrame components, so we
    // test them here instead of calling AXFrame directly.
    const [helloWidth, helloHeight] = hello.getAttributeValue("AXSize");
    const [worldWidth, worldHeight] = world.getAttributeValue("AXSize");
    ok(helloWidth > 0, "Hello has a positive width");
    ok(helloHeight > 0, "Hello has a positive height");
    ok(worldWidth > 0, "World has a positive width");
    ok(worldHeight > 0, "World has a positive height");
    ok(helloHeight < worldHeight, "Hello has a smaller height than world");
    ok(helloWidth < worldWidth, "Hello has a smaller width than world");

    // Note: these are mac screen coords, so our origin is bottom left
    const [helloX, helloY] = hello.getAttributeValue("AXPosition");
    const [worldX, worldY] = world.getAttributeValue("AXPosition");
    ok(helloX < 0, "Hello has a negative X");
    ok(helloY > 0, "Hello has a positive Y");
    ok(worldX < 0, "World has a negative X");
    ok(worldY > 0, "World has a positive Y");
    ok(helloY > worldY, "Hello has a larger Y than world");
    ok(helloX == worldX, "Hello and world have the same X");
  }
);
