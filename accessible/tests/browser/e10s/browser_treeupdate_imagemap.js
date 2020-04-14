/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

async function testImageMap(browser, accDoc) {
  const id = "imgmap";
  const acc = findAccessibleChildByID(accDoc, id);

  /* ================= Initial tree test ==================================== */
  let tree = {
    IMAGE_MAP: [{ role: ROLE_LINK, name: "b", children: [] }],
  };
  testAccessibleTree(acc, tree);

  /* ================= Insert area ========================================== */
  let onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [], () => {
    let areaElm = content.document.createElement("area");
    let mapNode = content.document.getElementById("map");
    areaElm.setAttribute(
      "href",
      "http://www.bbc.co.uk/radio4/atoz/index.shtml#a"
    );
    areaElm.setAttribute("coords", "0,0,13,14");
    areaElm.setAttribute("alt", "a");
    areaElm.setAttribute("shape", "rect");
    mapNode.insertBefore(areaElm, mapNode.firstChild);
  });
  await onReorder;

  tree = {
    IMAGE_MAP: [
      { role: ROLE_LINK, name: "a", children: [] },
      { role: ROLE_LINK, name: "b", children: [] },
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================= Append area ========================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [], () => {
    let areaElm = content.document.createElement("area");
    let mapNode = content.document.getElementById("map");
    areaElm.setAttribute(
      "href",
      "http://www.bbc.co.uk/radio4/atoz/index.shtml#c"
    );
    areaElm.setAttribute("coords", "34,0,47,14");
    areaElm.setAttribute("alt", "c");
    areaElm.setAttribute("shape", "rect");
    mapNode.appendChild(areaElm);
  });
  await onReorder;

  tree = {
    IMAGE_MAP: [
      { role: ROLE_LINK, name: "a", children: [] },
      { role: ROLE_LINK, name: "b", children: [] },
      { role: ROLE_LINK, name: "c", children: [] },
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================= Remove area ========================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [], () => {
    let mapNode = content.document.getElementById("map");
    mapNode.removeChild(mapNode.firstElementChild);
  });
  await onReorder;

  tree = {
    IMAGE_MAP: [
      { role: ROLE_LINK, name: "b", children: [] },
      { role: ROLE_LINK, name: "c", children: [] },
    ],
  };
  testAccessibleTree(acc, tree);
}

async function testContainer(browser) {
  const id = "container";
  /* ================= Remove name on map =================================== */
  let onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetAttribute(browser, "map", "name");
  let event = await onReorder;
  const acc = event.accessible;

  let tree = {
    SECTION: [{ GRAPHIC: [] }],
  };
  testAccessibleTree(acc, tree);

  /* ================= Restore name on map ================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetAttribute(browser, "map", "name", "atoz_map");
  // XXX: force repainting of the image (see bug 745788 for details).
  await invokeContentTask(browser, [], () => {
    const { ContentTaskUtils } = ChromeUtils.import(
      "resource://testing-common/ContentTaskUtils.jsm"
    );
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.synthesizeMouse(
      content.document.getElementById("imgmap"),
      10,
      10,
      { type: "mousemove" },
      content
    );
  });
  await onReorder;

  tree = {
    SECTION: [
      {
        IMAGE_MAP: [{ LINK: [] }, { LINK: [] }],
      },
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================= Remove map =========================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [], () => {
    let mapNode = content.document.getElementById("map");
    mapNode.remove();
  });
  await onReorder;

  tree = {
    SECTION: [{ GRAPHIC: [] }],
  };
  testAccessibleTree(acc, tree);

  /* ================= Insert map =========================================== */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeContentTask(browser, [id], contentId => {
    let map = content.document.createElement("map");
    let area = content.document.createElement("area");

    map.setAttribute("name", "atoz_map");
    map.setAttribute("id", "map");

    area.setAttribute("href", "http://www.bbc.co.uk/radio4/atoz/index.shtml#b");
    area.setAttribute("coords", "17,0,30,14");
    area.setAttribute("alt", "b");
    area.setAttribute("shape", "rect");

    map.appendChild(area);
    content.document.getElementById(contentId).appendChild(map);
  });
  await onReorder;

  tree = {
    SECTION: [
      {
        IMAGE_MAP: [{ LINK: [] }],
      },
    ],
  };
  testAccessibleTree(acc, tree);

  /* ================= Hide image map ======================================= */
  onReorder = waitForEvent(EVENT_REORDER, id);
  await invokeSetStyle(browser, "imgmap", "display", "none");
  await onReorder;

  tree = {
    SECTION: [],
  };
  testAccessibleTree(acc, tree);
}

addAccessibleTask(
  "e10s/doc_treeupdate_imagemap.html",
  async function(browser, accDoc) {
    await waitForImageMap(browser, accDoc);
    await testImageMap(browser, accDoc);
    await testContainer(browser);
  },
  { iframe: true, remoteIframe: true }
);
