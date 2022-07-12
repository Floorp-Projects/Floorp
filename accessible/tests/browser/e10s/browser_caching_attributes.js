/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

/**
 * Default textbox accessible attributes.
 */
const defaultAttributes = {
  "margin-top": "0px",
  "margin-right": "0px",
  "margin-bottom": "0px",
  "margin-left": "0px",
  "text-align": "start",
  "text-indent": "0px",
  id: "textbox",
  tag: "input",
  display: "inline-block",
};

/**
 * Test data has the format of:
 * {
 *   desc        {String}         description for better logging
 *   expected    {Object}         expected attributes for given accessibles
 *   unexpected  {Object}         unexpected attributes for given accessibles
 *
 *   action      {?AsyncFunction} an optional action that awaits a change in
 *                                attributes
 *   attrs       {?Array}         an optional list of attributes to update
 *   waitFor     {?Number}        an optional event to wait for
 * }
 */
const attributesTests = [
  {
    desc: "Initiall accessible attributes",
    expected: defaultAttributes,
    unexpected: {
      "line-number": "1",
      "explicit-name": "true",
      "container-live": "polite",
      live: "polite",
    },
  },
  {
    desc: "@line-number attribute is present when textbox is focused",
    async action(browser) {
      await invokeFocus(browser, "textbox");
    },
    waitFor: EVENT_FOCUS,
    expected: Object.assign({}, defaultAttributes, { "line-number": "1" }),
    unexpected: {
      "explicit-name": "true",
      "container-live": "polite",
      live: "polite",
    },
  },
  {
    desc: "@aria-live sets container-live and live attributes",
    attrs: [
      {
        attr: "aria-live",
        value: "polite",
      },
    ],
    expected: Object.assign({}, defaultAttributes, {
      "line-number": "1",
      "container-live": "polite",
      live: "polite",
    }),
    unexpected: {
      "explicit-name": "true",
    },
  },
  {
    desc: "@title attribute sets explicit-name attribute to true",
    attrs: [
      {
        attr: "title",
        value: "textbox",
      },
    ],
    expected: Object.assign({}, defaultAttributes, {
      "line-number": "1",
      "explicit-name": "true",
      "container-live": "polite",
      live: "polite",
    }),
    unexpected: {},
  },
];

/**
 * Test caching of accessible object attributes
 */
addAccessibleTask(
  `
  <input id="textbox" value="hello">`,
  async function(browser, accDoc) {
    let textbox = findAccessibleChildByID(accDoc, "textbox");
    for (let {
      desc,
      action,
      attrs,
      expected,
      waitFor,
      unexpected,
    } of attributesTests) {
      info(desc);
      let onUpdate;

      if (waitFor) {
        onUpdate = waitForEvent(waitFor, "textbox");
      }

      if (action) {
        await action(browser);
      } else if (attrs) {
        for (let { attr, value } of attrs) {
          await invokeSetAttribute(browser, "textbox", attr, value);
        }
      }

      await onUpdate;
      testAttrs(textbox, expected);
      testAbsentAttrs(textbox, unexpected);
    }
  },
  {
    // These tests don't work yet with the parent process cache enabled.
    topLevel: !isCacheEnabled,
    iframe: !isCacheEnabled,
    remoteIframe: !isCacheEnabled,
  }
);

/**
 * Test caching of the tag attribute.
 */
addAccessibleTask(
  `
<p id="p">text</p>
<textarea id="textarea"></textarea>
  `,
  async function(browser, docAcc) {
    testAttrs(docAcc, { tag: "body" }, true);
    const p = findAccessibleChildByID(docAcc, "p");
    testAttrs(p, { tag: "p" }, true);
    const textLeaf = p.firstChild;
    testAbsentAttrs(textLeaf, { tag: "" });
    const textarea = findAccessibleChildByID(docAcc, "textarea");
    testAttrs(textarea, { tag: "textarea" }, true);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test caching of the text-input-type attribute.
 */
addAccessibleTask(
  `
  <input id="default">
  <input id="email" type="email">
  <input id="password" type="password">
  <input id="text" type="text">
  <input id="date" type="date">
  <input id="time" type="time">
  <input id="checkbox" type="checkbox">
  <input id="radio" type="radio">
  `,
  async function(browser, docAcc) {
    function testInputType(id, inputType) {
      if (inputType == undefined) {
        testAbsentAttrs(findAccessibleChildByID(docAcc, id), {
          "text-input-type": "",
        });
      } else {
        testAttrs(
          findAccessibleChildByID(docAcc, id),
          { "text-input-type": inputType },
          true
        );
      }
    }

    testInputType("default");
    testInputType("email", "email");
    testInputType("password", "password");
    testInputType("text", "text");
    testInputType("date", "date");
    testInputType("time", "time");
    testInputType("checkbox");
    testInputType("radio");
  },
  { chrome: true, topLevel: true, iframe: false, remoteIframe: false }
);

/**
 * Test caching of the display attribute.
 */
addAccessibleTask(
  `
<div id="div">
  <ins id="ins">a</ins>
  <button id="button">b</button>
</div>
  `,
  async function(browser, docAcc) {
    const div = findAccessibleChildByID(docAcc, "div");
    testAttrs(div, { display: "block" }, true);
    const ins = findAccessibleChildByID(docAcc, "ins");
    testAttrs(ins, { display: "inline" }, true);
    const textLeaf = ins.firstChild;
    testAbsentAttrs(textLeaf, { display: "" });
    const button = findAccessibleChildByID(docAcc, "button");
    testAttrs(button, { display: "inline-block" }, true);

    await invokeContentTask(browser, [], () => {
      content.document.getElementById("ins").style.display = "block";
      content.document.body.offsetTop; // Flush layout.
    });
    await untilCacheIs(
      () => ins.attributes.getStringProperty("display"),
      "block",
      "ins display attribute changed to block"
    );
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test caching of the explicit-name attribute.
 */
addAccessibleTask(
  `
<h1 id="h1">content</h1>
<button id="buttonContent">content</button>
<button id="buttonLabel" aria-label="label">content</button>
<button id="buttonEmpty"></button>
<button id="buttonSummary"><details><summary>test</summary></details></button>
<div id="div"></div>
  `,
  async function(browser, docAcc) {
    const h1 = findAccessibleChildByID(docAcc, "h1");
    testAbsentAttrs(h1, { "explicit-name": "" });
    const buttonContent = findAccessibleChildByID(docAcc, "buttonContent");
    testAbsentAttrs(buttonContent, { "explicit-name": "" });
    const buttonLabel = findAccessibleChildByID(docAcc, "buttonLabel");
    testAttrs(buttonLabel, { "explicit-name": "true" }, true);
    const buttonEmpty = findAccessibleChildByID(docAcc, "buttonEmpty");
    testAbsentAttrs(buttonEmpty, { "explicit-name": "" });
    const buttonSummary = findAccessibleChildByID(docAcc, "buttonSummary");
    testAbsentAttrs(buttonSummary, { "explicit-name": "" });
    const div = findAccessibleChildByID(docAcc, "div");
    testAbsentAttrs(div, { "explicit-name": "" });

    info("Setting aria-label on h1");
    let nameChanged = waitForEvent(EVENT_NAME_CHANGE, h1);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("h1").setAttribute("aria-label", "label");
    });
    await nameChanged;
    testAttrs(h1, { "explicit-name": "true" }, true);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test caching of ARIA attributes that are exposed via object attributes.
 */
addAccessibleTask(
  `
<div id="currentTrue" aria-current="true">currentTrue</div>
<div id="currentFalse" aria-current="false">currentFalse</div>
<div id="currentPage" aria-current="page">currentPage</div>
<div id="currentBlah" aria-current="blah">currentBlah</div>
<div id="haspopupMenu" aria-haspopup="menu">haspopup</div>
<div id="rowColCountPositive" role="table" aria-rowcount="1000" aria-colcount="1000">
  <div role="row">
    <div id="rowColIndexPositive" role="cell" aria-rowindex="100" aria-colindex="100">positive</div>
  </div>
</div>
<div id="rowColCountNegative" role="table" aria-rowcount="-1" aria-colcount="-1">
  <div role="row">
    <div id="rowColIndexNegative" role="cell" aria-rowindex="-1" aria-colindex="-1">negative</div>
  </div>
</div>
<div id="rowColCountInvalid" role="table" aria-rowcount="z" aria-colcount="z">
  <div role="row">
    <div id="rowColIndexInvalid" role="cell" aria-rowindex="z" aria-colindex="z">invalid</div>
  </div>
</div>
<div id="foo" aria-foo="bar">foo</div>
<div id="mutate" aria-current="true">mutate</div>
  `,
  async function(browser, docAcc) {
    const currentTrue = findAccessibleChildByID(docAcc, "currentTrue");
    testAttrs(currentTrue, { current: "true" }, true);
    const currentFalse = findAccessibleChildByID(docAcc, "currentFalse");
    testAbsentAttrs(currentFalse, { current: "" });
    const currentPage = findAccessibleChildByID(docAcc, "currentPage");
    testAttrs(currentPage, { current: "page" }, true);
    // Test that token normalization works.
    const currentBlah = findAccessibleChildByID(docAcc, "currentBlah");
    testAttrs(currentBlah, { current: "true" }, true);
    const haspopupMenu = findAccessibleChildByID(docAcc, "haspopupMenu");
    testAttrs(haspopupMenu, { haspopup: "menu" }, true);

    // Test normalization of integer values.
    const rowColCountPositive = findAccessibleChildByID(
      docAcc,
      "rowColCountPositive"
    );
    testAttrs(
      rowColCountPositive,
      { rowcount: "1000", colcount: "1000" },
      true
    );
    const rowColIndexPositive = findAccessibleChildByID(
      docAcc,
      "rowColIndexPositive"
    );
    testAttrs(rowColIndexPositive, { rowindex: "100", colindex: "100" }, true);
    const rowColCountNegative = findAccessibleChildByID(
      docAcc,
      "rowColCountNegative"
    );
    testAttrs(rowColCountNegative, { rowcount: "-1", colcount: "-1" }, true);
    const rowColIndexNegative = findAccessibleChildByID(
      docAcc,
      "rowColIndexNegative"
    );
    testAbsentAttrs(rowColIndexNegative, { rowindex: "", colindex: "" });
    const rowColCountInvalid = findAccessibleChildByID(
      docAcc,
      "rowColCountInvalid"
    );
    testAbsentAttrs(rowColCountInvalid, { rowcount: "", colcount: "" });
    const rowColIndexInvalid = findAccessibleChildByID(
      docAcc,
      "rowColIndexInvalid"
    );
    testAbsentAttrs(rowColIndexInvalid, { rowindex: "", colindex: "" });

    // Test that unknown aria- attributes get exposed.
    const foo = findAccessibleChildByID(docAcc, "foo");
    testAttrs(foo, { foo: "bar" }, true);

    const mutate = findAccessibleChildByID(docAcc, "mutate");
    testAttrs(mutate, { current: "true" }, true);
    info("mutate: Removing aria-current");
    let changed = waitForEvent(EVENT_OBJECT_ATTRIBUTE_CHANGED, mutate);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("mutate").removeAttribute("aria-current");
    });
    await changed;
    testAbsentAttrs(mutate, { current: "" });
    info("mutate: Adding aria-current");
    changed = waitForEvent(EVENT_OBJECT_ATTRIBUTE_CHANGED, mutate);
    await invokeContentTask(browser, [], () => {
      content.document
        .getElementById("mutate")
        .setAttribute("aria-current", "page");
    });
    await changed;
    testAttrs(mutate, { current: "page" }, true);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test support for the xml-roles attribute.
 */
addAccessibleTask(
  `
<div id="knownRole" role="main">knownRole</div>
<div id="emptyRole" role="">emptyRole</div>
<div id="unknownRole" role="foo">unknownRole</div>
<div id="multiRole" role="foo main">multiRole</div>
<main id="landmarkMarkup">landmarkMarkup</main>
<main id="landmarkMarkupWithRole" role="banner">landmarkMarkupWithRole</main>
<main id="landmarkMarkupWithEmptyRole" role="">landmarkMarkupWithEmptyRole</main>
<article id="markup">markup</article>
<article id="markupWithRole" role="banner">markupWithRole</article>
<article id="markupWithEmptyRole" role="">markupWithEmptyRole</article>
  `,
  async function(browser, docAcc) {
    const knownRole = findAccessibleChildByID(docAcc, "knownRole");
    testAttrs(knownRole, { "xml-roles": "main" }, true);
    const emptyRole = findAccessibleChildByID(docAcc, "emptyRole");
    testAbsentAttrs(emptyRole, { "xml-roles": "" });
    const unknownRole = findAccessibleChildByID(docAcc, "unknownRole");
    testAttrs(unknownRole, { "xml-roles": "foo" }, true);
    const multiRole = findAccessibleChildByID(docAcc, "multiRole");
    testAttrs(multiRole, { "xml-roles": "foo main" }, true);
    const landmarkMarkup = findAccessibleChildByID(docAcc, "landmarkMarkup");
    testAttrs(landmarkMarkup, { "xml-roles": "main" }, true);
    const landmarkMarkupWithRole = findAccessibleChildByID(
      docAcc,
      "landmarkMarkupWithRole"
    );
    testAttrs(landmarkMarkupWithRole, { "xml-roles": "banner" }, true);
    const landmarkMarkupWithEmptyRole = findAccessibleChildByID(
      docAcc,
      "landmarkMarkupWithEmptyRole"
    );
    testAttrs(landmarkMarkupWithEmptyRole, { "xml-roles": "main" }, true);
    const markup = findAccessibleChildByID(docAcc, "markup");
    testAttrs(markup, { "xml-roles": "article" }, true);
    const markupWithRole = findAccessibleChildByID(docAcc, "markupWithRole");
    testAttrs(markupWithRole, { "xml-roles": "banner" }, true);
    const markupWithEmptyRole = findAccessibleChildByID(
      docAcc,
      "markupWithEmptyRole"
    );
    testAttrs(markupWithEmptyRole, { "xml-roles": "article" }, true);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test lie region attributes.
 */
addAccessibleTask(
  `
<div id="noLive"><p>noLive</p></div>
<output id="liveMarkup"><p>liveMarkup</p></output>
<div id="ariaLive" aria-live="polite"><p>ariaLive</p></div>
<div id="liveRole" role="log"><p>liveRole</p></div>
<div id="nonLiveRole" role="group"><p>nonLiveRole</p></div>
<div id="other" aria-atomic="true" aria-busy="true" aria-relevant="additions"><p>other</p></div>
  `,
  async function(browser, docAcc) {
    const noLive = findAccessibleChildByID(docAcc, "noLive");
    for (const acc of [noLive, noLive.firstChild]) {
      testAbsentAttrs(acc, {
        live: "",
        "container-live": "",
        "container-live-role": "",
        atomic: "",
        "container-atomic": "",
        busy: "",
        "container-busy": "",
        relevant: "",
        "container-relevant": "",
      });
    }
    const liveMarkup = findAccessibleChildByID(docAcc, "liveMarkup");
    testAttrs(liveMarkup, { live: "polite" }, true);
    testAttrs(liveMarkup.firstChild, { "container-live": "polite" }, true);
    const ariaLive = findAccessibleChildByID(docAcc, "ariaLive");
    testAttrs(ariaLive, { live: "polite" }, true);
    testAttrs(ariaLive.firstChild, { "container-live": "polite" }, true);
    const liveRole = findAccessibleChildByID(docAcc, "liveRole");
    testAttrs(liveRole, { live: "polite" }, true);
    testAttrs(
      liveRole.firstChild,
      { "container-live": "polite", "container-live-role": "log" },
      true
    );
    const nonLiveRole = findAccessibleChildByID(docAcc, "nonLiveRole");
    testAbsentAttrs(nonLiveRole, { live: "" });
    testAbsentAttrs(nonLiveRole.firstChild, {
      "container-live": "",
      "container-live-role": "",
    });
    const other = findAccessibleChildByID(docAcc, "other");
    testAttrs(
      other,
      { atomic: "true", busy: "true", relevant: "additions" },
      true
    );
    testAttrs(
      other.firstChild,
      {
        "container-atomic": "true",
        "container-busy": "true",
        "container-relevant": "additions",
      },
      true
    );
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
