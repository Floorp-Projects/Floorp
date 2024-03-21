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

async function synthFocus(browser, container, item) {
  let focusPromise = waitForEvent(EVENT_FOCUS, item);
  await invokeContentTask(browser, [container], _container => {
    let elm = (
      content.document._testGetElementById || content.document.getElementById
    ).bind(content.document)(_container);
    elm.focus();
  });
  await focusPromise;
}

async function changeARIAActiveDescendant(
  browser,
  container,
  itemId,
  prevItemId,
  elementReflection
) {
  let expectedEvents = [[EVENT_FOCUS, itemId]];

  if (prevItemId) {
    info("A state change of the previous item precedes the new one.");
    expectedEvents.push(
      stateChangeEventArgs(prevItemId, EXT_STATE_ACTIVE, false, true)
    );
  }

  expectedEvents.push(
    stateChangeEventArgs(itemId, EXT_STATE_ACTIVE, true, true)
  );

  let expectedPromise = waitForEvents(expectedEvents);
  await invokeContentTask(
    browser,
    [container, itemId, elementReflection],
    (_container, _itemId, _elementReflection) => {
      let getElm = (
        content.document._testGetElementById || content.document.getElementById
      ).bind(content.document);
      let elm = getElm(_container);
      if (_elementReflection) {
        elm.ariaActiveDescendantElement = getElm(_itemId);
      } else {
        elm.setAttribute("aria-activedescendant", _itemId);
      }
    }
  );

  await expectedPromise;
}

async function clearARIAActiveDescendant(
  browser,
  container,
  prevItemId,
  defaultId,
  elementReflection
) {
  let expectedEvents = [[EVENT_FOCUS, defaultId || container]];
  if (prevItemId) {
    expectedEvents.push(
      stateChangeEventArgs(prevItemId, EXT_STATE_ACTIVE, false, true)
    );
  }

  if (defaultId) {
    expectedEvents.push(
      stateChangeEventArgs(defaultId, EXT_STATE_ACTIVE, true, true)
    );
  }

  let expectedPromise = waitForEvents(expectedEvents);
  await invokeContentTask(
    browser,
    [container, elementReflection],
    (_container, _elementReflection) => {
      let elm = (
        content.document._testGetElementById || content.document.getElementById
      ).bind(content.document)(_container);
      if (_elementReflection) {
        elm.ariaActiveDescendantElement = null;
      } else {
        elm.removeAttribute("aria-activedescendant");
      }
    }
  );

  await expectedPromise;
}

async function insertItemNFocus(
  browser,
  container,
  newItemID,
  prevItemId,
  elementReflection
) {
  let expectedEvents = [
    [EVENT_SHOW, newItemID],
    [EVENT_FOCUS, newItemID],
  ];

  if (prevItemId) {
    info("A state change of the previous item precedes the new one.");
    expectedEvents.push(
      stateChangeEventArgs(prevItemId, EXT_STATE_ACTIVE, false, true)
    );
  }

  expectedEvents.push(
    stateChangeEventArgs(newItemID, EXT_STATE_ACTIVE, true, true)
  );

  let expectedPromise = waitForEvents(expectedEvents);

  await invokeContentTask(
    browser,
    [container, newItemID, elementReflection],
    (_container, _newItemID, _elementReflection) => {
      let elm = (
        content.document._testGetElementById || content.document.getElementById
      ).bind(content.document)(_container);
      let itemElm = content.document.createElement("div");
      itemElm.setAttribute("id", _newItemID);
      itemElm.setAttribute("role", "listitem");
      itemElm.textContent = _newItemID;
      elm.appendChild(itemElm);
      if (_elementReflection) {
        elm.ariaActiveDescendantElement = itemElm;
      } else {
        elm.setAttribute("aria-activedescendant", _newItemID);
      }
    }
  );

  await expectedPromise;
}

async function moveARIAActiveDescendantID(browser, fromID, toID) {
  let expectedEvents = [
    [EVENT_FOCUS, toID],
    stateChangeEventArgs(toID, EXT_STATE_ACTIVE, true, true),
  ];

  let expectedPromise = waitForEvents(expectedEvents);
  await invokeContentTask(browser, [fromID, toID], (_fromID, _toID) => {
    let orig = (
      content.document._testGetElementById || content.document.getElementById
    ).bind(content.document)(_toID);
    if (orig) {
      orig.id = "";
    }
    (
      content.document._testGetElementById || content.document.getElementById
    ).bind(content.document)(_fromID).id = _toID;
  });
  await expectedPromise;
}

async function changeARIAActiveDescendantInvalid(
  browser,
  container,
  invalidID = "invalid",
  prevItemId = null
) {
  let expectedEvents = [[EVENT_FOCUS, container]];
  if (prevItemId) {
    expectedEvents.push(
      stateChangeEventArgs(prevItemId, EXT_STATE_ACTIVE, false, true)
    );
  }

  let expectedPromise = waitForEvents(expectedEvents);
  await invokeContentTask(
    browser,
    [container, invalidID],
    (_container, _invalidID) => {
      let elm = (
        content.document._testGetElementById || content.document.getElementById
      ).bind(content.document)(_container);
      elm.setAttribute("aria-activedescendant", _invalidID);
    }
  );

  await expectedPromise;
}

const LISTBOX_MARKUP = `
<div role="listbox" aria-activedescendant="item1" id="listbox" tabindex="1"
aria-owns="item3">
<div role="listitem" id="item1">item1</div>
<div role="listitem" id="item2">item2</div>
<div role="listitem" id="roaming" data-id="roaming">roaming</div>
<div role="listitem" id="roaming2" data-id="roaming2">roaming2</div>
</div>
<div role="listitem" id="item3">item3</div>
<div role="combobox" id="combobox">
<input id="combobox_entry">
<ul>
  <li role="option" id="combobox_option1">option1</li>
  <li role="option" id="combobox_option2">option2</li>
</ul>
</div>`;

async function basicListboxTest(browser, elementReflection) {
  await synthFocus(browser, "listbox", "item1");
  await changeARIAActiveDescendant(
    browser,
    "listbox",
    "item2",
    "item1",
    elementReflection
  );
  await changeARIAActiveDescendant(
    browser,
    "listbox",
    "item3",
    "item2",
    elementReflection
  );

  info("Focus out of listbox");
  await synthFocus(browser, "combobox_entry", "combobox_entry");
  await changeARIAActiveDescendant(
    browser,
    "combobox",
    "combobox_option2",
    null,
    elementReflection
  );
  await changeARIAActiveDescendant(
    browser,
    "combobox",
    "combobox_option1",
    null,
    elementReflection
  );

  info("Focus back in listbox");
  await synthFocus(browser, "listbox", "item3");
  await insertItemNFocus(
    browser,
    "listbox",
    "item4",
    "item3",
    elementReflection
  );

  await clearARIAActiveDescendant(
    browser,
    "listbox",
    "item4",
    null,
    elementReflection
  );
  await changeARIAActiveDescendant(
    browser,
    "listbox",
    "item1",
    null,
    elementReflection
  );
}

addAccessibleTask(
  LISTBOX_MARKUP,
  async function (browser) {
    info("Test aria-activedescendant content attribute");
    await basicListboxTest(browser, false);

    await changeARIAActiveDescendantInvalid(
      browser,
      "listbox",
      "invalid",
      "item1"
    );

    await changeARIAActiveDescendant(browser, "listbox", "roaming");
    await moveARIAActiveDescendantID(browser, "roaming2", "roaming");
    await changeARIAActiveDescendantInvalid(
      browser,
      "listbox",
      "roaming3",
      "roaming"
    );
    await moveARIAActiveDescendantID(browser, "roaming", "roaming3");
  },
  { topLevel: true, chrome: true }
);

addAccessibleTask(
  LISTBOX_MARKUP,
  async function (browser) {
    info("Test ariaActiveDescendantElement element reflection");
    await basicListboxTest(browser, true);
  },
  { topLevel: true, chrome: true }
);

addAccessibleTask(
  `
<input id="activedesc_nondesc_input" aria-activedescendant="activedesc_nondesc_option">
<div role="listbox">
  <div role="option" id="activedesc_nondesc_option">option</div>
</div>`,
  async function (browser) {
    info("Test aria-activedescendant non-descendant");
    await synthFocus(
      browser,
      "activedesc_nondesc_input",
      "activedesc_nondesc_option"
    );
  },
  { topLevel: true, chrome: true }
);

addAccessibleTask(
  `
<div id="shadow"></div>
<script>
  let host = document.getElementById("shadow");
  let shadow = host.attachShadow({mode: "open"});
  let listbox = document.createElement("div");
  listbox.id = "shadowListbox";
  listbox.setAttribute("role", "listbox");
  listbox.setAttribute("tabindex", "0");
  shadow.appendChild(listbox);
  let item = document.createElement("div");
  item.id = "shadowItem1";
  item.setAttribute("role", "option");
  listbox.appendChild(item);
  listbox.setAttribute("aria-activedescendant", "shadowItem1");
  item = document.createElement("div");
  item.id = "shadowItem2";
  item.setAttribute("role", "option");
  listbox.appendChild(item);
</script>`,
  async function (browser) {
    info("Test aria-activedescendant in shadow root");
    // We want to retrieve elements using their IDs inside the shadow root, so
    // we define a custom get element by ID method that our utility functions
    // above call into if it exists.
    await invokeContentTask(browser, [], () => {
      content.document._testGetElementById = id =>
        content.document.getElementById("shadow").shadowRoot.getElementById(id);
    });

    await synthFocus(browser, "shadowListbox", "shadowItem1");
    await changeARIAActiveDescendant(
      browser,
      "shadowListbox",
      "shadowItem2",
      "shadowItem1"
    );
    info("Do it again with element reflection");
    await changeARIAActiveDescendant(
      browser,
      "shadowListbox",
      "shadowItem1",
      "shadowItem2",
      true
    );
  },
  { topLevel: true, chrome: true }
);

addAccessibleTask(
  `
<div id="comboboxWithHiddenList" tabindex="0" role="combobox" aria-owns="hiddenList">
</div>
<div id="hiddenList" hidden role="listbox">
  <div id="hiddenListOption" role="option"></div>
</div>`,
  async function (browser, docAcc) {
    info("Test simultaneous insertion, relocation and aria-activedescendant");
    await synthFocus(
      browser,
      "comboboxWithHiddenList",
      "comboboxWithHiddenList"
    );

    testStates(
      findAccessibleChildByID(docAcc, "comboboxWithHiddenList"),
      STATE_FOCUSED
    );
    let evtProm = Promise.all([
      waitForEvent(EVENT_FOCUS, "hiddenListOption"),
      waitForStateChange("hiddenListOption", EXT_STATE_ACTIVE, true, true),
    ]);
    await invokeContentTask(browser, [], () => {
      info("hiddenList is owned, so unhiding causes insertion and relocation.");
      (
        content.document._testGetElementById || content.document.getElementById
      ).bind(content.document)("hiddenList").hidden = false;
      content.document
        .getElementById("comboboxWithHiddenList")
        .setAttribute("aria-activedescendant", "hiddenListOption");
    });
    await evtProm;
    testStates(
      findAccessibleChildByID(docAcc, "hiddenListOption"),
      STATE_FOCUSED
    );
  },
  { topLevel: true, chrome: true }
);

addAccessibleTask(
  `
<custom-listbox id="custom-listbox1">
  <div role="listitem" id="l1_1"></div>
  <div role="listitem" id="l1_2"></div>
  <div role="listitem" id="l1_3"></div>
</custom-listbox>

<custom-listbox id="custom-listbox2" aria-activedescendant="l2_1">
  <div role="listitem" id="l2_1"></div>
  <div role="listitem" id="l2_2"></div>
  <div role="listitem" id="l2_3"></div>
</custom-listbox>

<script>
customElements.define("custom-listbox",
  class extends HTMLElement {
    constructor() {
      super();
      this.tabIndex = "0"
      this._internals = this.attachInternals();
      this._internals.role = "listbox";
      this._internals.ariaActiveDescendantElement = this.lastElementChild;
    }
    get internals() {
      return this._internals;
    }
  }
);
</script>`,
  async function (browser) {
    await synthFocus(browser, "custom-listbox1", "l1_3");

    let evtProm = Promise.all([
      waitForEvent(EVENT_FOCUS, "l1_2"),
      waitForStateChange("l1_3", EXT_STATE_ACTIVE, false, true),
      waitForStateChange("l1_2", EXT_STATE_ACTIVE, true, true),
    ]);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById(
        "custom-listbox1"
      ).internals.ariaActiveDescendantElement =
        content.document.getElementById("l1_2");
    });

    await evtProm;

    evtProm = Promise.all([
      waitForEvent(EVENT_FOCUS, "custom-listbox1"),
      waitForStateChange("l1_2", EXT_STATE_ACTIVE, false, true),
    ]);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById(
        "custom-listbox1"
      ).internals.ariaActiveDescendantElement = null;
    });

    await evtProm;

    await synthFocus(browser, "custom-listbox2", "l2_1");
    await clearARIAActiveDescendant(browser, "custom-listbox2", "l2_1", "l2_3");
  }
);
