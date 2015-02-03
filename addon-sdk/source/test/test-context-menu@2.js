/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");
const {openWindow, closeWindow, openTab, closeTab,
       openContextMenu, closeContextMenu, select,
       readNode, captureContextMenu, withTab, withItems } = require("./context-menu/util");
const {when} = require("sdk/dom/events");
const {Item, Menu, Separator, Contexts, Readers } = require("sdk/context-menu@2");
const prefs = require("sdk/preferences/service");

const testPageURI = require.resolve("./test-context-menu").replace(".js", ".html");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const data = input =>
  `data:text/html;charset=utf-8,${encodeURIComponent(input)}`

const menugroup = (...children) => Object.assign({
  tagName: "menugroup",
  namespaceURI: XUL_NS,
  style: "-moz-box-orient: vertical;",
  className: "sdk-context-menu-extension"
}, children.length ? {children} : {});

const menuseparator = () => ({
  tagName: "menuseparator",
  namespaceURI: XUL_NS,
  className: "sdk-context-menu-separator"
})

const menuitem = properties => Object.assign({
  tagName: "menuitem",
  namespaceURI: XUL_NS,
  className: "sdk-context-menu-item menuitem-iconic"
}, properties);

const menu = (properties, ...children) => Object.assign({
  tagName: "menu",
  namespaceURI: XUL_NS,
  className: "sdk-context-menu menu-iconic"
}, properties, {
  children: [Object.assign({tagName: "menupopup", namespaceURI: XUL_NS},
                           children.length ? {children} : {})]
});

// Destroying items that were previously created should cause them to be absent
// from the menu.
exports["test create / destroy menu item"] = withTab(function*(assert) {
  const item = new Item({
    label: "test-1"
  });

  const before = yield captureContextMenu("h1");

  assert.deepEqual(before,
                   menugroup(menuseparator(),
                             menuitem({label: "test-1"})),
                   "context menu contains separator & added item");

  item.destroy();

  const after = yield captureContextMenu("h1");
  assert.deepEqual(after, menugroup(),
                   "all items were removed children are present");
}, data`<h1>hello</h1>`);


/* Bug 1115419 - Disable occasionally failing test until we
                 figure out why it fails.
// Items created should be present on all browser windows.
exports["test menu item in new window"] = function*(assert) {
  const isMenuPopulated = function*(tab) {
    const state = yield captureContextMenu("h1", tab);
    assert.deepEqual(state,
                     menugroup(menuseparator(),
                               menuitem({label: "multi-window"})),
                     "created menu item is present")
  };

  const isMenuEmpty = function*(tab) {
    const state = yield captureContextMenu("h1", tab);
    assert.deepEqual(state, menugroup(), "no sdk items present");
  };

  const item = new Item({ label: "multi-window" });

  const tab1 = yield openTab(`data:text/html,<h1>hello</h1>`);
  yield* isMenuPopulated(tab1);

  const window2 = yield openWindow();
  assert.pass("window is ready");

  const tab2 = yield openTab(`data:text/html,<h1>hello window-2</h1>`, window2);
  assert.pass("tab is ready");

  yield* isMenuPopulated(tab2);

  item.destroy();

  yield* isMenuEmpty(tab2);
  yield closeWindow(window2);

  yield* isMenuEmpty(tab1);

  yield closeTab(tab1);
};
*/


// Multilpe items can be created and destroyed at different points
// in time & they should not affect each other.
exports["test multiple items"] = withTab(function*(assert) {
  const item1 = new Item({ label: "one" });

  const step1 = yield captureContextMenu("h1");
  assert.deepEqual(step1,
                   menugroup(menuseparator(),
                             menuitem({label: "one"})),
                   "item1 is present");

  const item2 = new Item({ label: "two" });
  const step2 = yield captureContextMenu("h1");

  assert.deepEqual(step2,
                   menugroup(menuseparator(),
                             menuitem({label: "one"}),
                             menuitem({label: "two"})),
                   "both items where present");

  item1.destroy();

  const step3 = yield captureContextMenu("h1");
  assert.deepEqual(step3,
                   menugroup(menuseparator(),
                             menuitem({label: "two"})),
                   "one items left");

  item2.destroy();

  const step4 = yield captureContextMenu("h1");
  assert.deepEqual(step4, menugroup(), "no items left");
}, data`<h1>Multiple Items</h1>`);

// Destroying an item twice should not cause an error.
exports["test destroy twice"] = withTab(function*(assert) {
  const item = new Item({ label: "destroy" });
  const withItem = yield captureContextMenu("h2");
  assert.deepEqual(withItem,
                   menugroup(menuseparator(),
                             menuitem({label:"destroy"})),
                   "Item is added");

  item.destroy();

  const withoutItem = yield captureContextMenu("h2");
  assert.deepEqual(withoutItem, menugroup(), "Item was removed");

  item.destroy();
  assert.pass("Destroying an item twice should not cause an error.");
}, "data:text/html,<h2>item destroy</h2>");

// CSS selector contexts should cause their items to be absent from the menu
// when the menu is not invoked on nodes that match selectors.
exports["test selector context"] = withTab(function*(assert) {
  const item = new Item({
    context: [new Contexts.Selector("body b")],
    label: "bold"
  });

  const match = yield captureContextMenu("b");
  assert.deepEqual(match,
                   menugroup(menuseparator(),
                             menuitem({label: "bold"})),
                   "item mathched context");

  const noMatch = yield captureContextMenu("i");
  assert.deepEqual(noMatch, menugroup(), "item did not match context");

  item.destroy();

  const cleared = yield captureContextMenu("b");
  assert.deepEqual(cleared, menugroup(), "item was removed");
}, data`<body><i>one</i><b>two</b></body>`);

// CSS selector contexts should cause their items to be absent in the menu
// when the menu is invoked even on nodes that have ancestors that match the
// selectors.
exports["test parent selector don't match children"] = withTab(function*(assert) {
  const item = new Item({
    label: "parent match",
    context: [new Contexts.Selector("a[href]")]
  });

  const match = yield captureContextMenu("a");
  assert.deepEqual(match,
                   menugroup(menuseparator(),
                             menuitem({label: "parent match"})),
                   "item mathched context");

  const noMatch = yield captureContextMenu("strong");
  assert.deepEqual(noMatch, menugroup(), "item did not mathch context");

  item.destroy();

  const destroyed = yield captureContextMenu("a");
  assert.deepEqual(destroyed, menugroup(), "no items left");
}, data`<a href='/foo'>This text must be long & <strong>bold!</strong></a>`);

// Page contexts should cause their items to be present in the menu when the
// menu is not invoked on an active element.
exports["test page context match"] = withTab(function*(assert) {
  const isPageMatch = (tree, description="page context matched") =>
    assert.deepEqual(tree,
                     menugroup(menuseparator(),
                               menuitem({label: "page match"}),
                               menuitem({label: "any match"})),
                     description);

  const isntPageMatch = (tree, description="page context did not match") =>
    assert.deepEqual(tree,
                     menugroup(menuseparator(),
                               menuitem({label: "any match"})),
                    description);

  yield* withItems({
    pageMatch: new Item({
      label: "page match",
      context: [new Contexts.Page()],
    }),
    anyMatch: new Item({
      label: "any match"
    })
  }, function*({pageMatch, anyMatch}) {
    for (let tagName of [null, "p", "h3"]) {
      isPageMatch((yield captureContextMenu(tagName)),
                  `Page context matches ${tagName} passive element`);
    }

    for (let tagName of ["button", "canvas", "img", "input", "textarea",
                         "select", "menu", "embed" ,"object", "video", "audio",
                         "applet"])
    {
      isntPageMatch((yield captureContextMenu(tagName)),
                    `Page context does not match <${tagName}/> active element`);
    }

    for (let selector of ["span"])
    {
      isntPageMatch((yield captureContextMenu(selector)),
                    `Page context does not match decedents of active element`);
    }
  });
},
data`<head>
  <style>
    p, object, embed { display: inline-block; }
  </style>
</head>
<body>
  <div><p>paragraph</p></div>
  <div><a href=./link><span>link</span></a></div>
  <h3>hi</h3>
  <div><button>button</button></div>
  <div><canvas height=10 /></div>
  <div><img height=10 width=10 /></div>
  <div><input value=input /></div>
  <div><textarea>text</textarea></div>
  <div><select><option>one</option><option>two</option></select></div>
  <div><menu><button>item</button></menu></div>
  <div><object width=10 height=10><param name=foo value=bar /></object></div>
  <div><embed width=10 height=10/></div>
  <div><video width=10 height=10 controls /></div>
  <div><audio width=10 height=10 controls /></div>
  <div><applet width=10 height=10 /></div>
</body>`);

// Page context does not match if if there is a selection.
exports["test page context doesn't match on selection"] = withTab(function*(assert) {
  const isPageMatch = (tree, description="page context matched") =>
    assert.deepEqual(tree,
                     menugroup(menuseparator(),
                               menuitem({label: "page match"}),
                               menuitem({label: "any match"})),
                     description);

  const isntPageMatch = (tree, description="page context did not match") =>
    assert.deepEqual(tree,
                     menugroup(menuseparator(),
                               menuitem({label: "any match"})),
                    description);

  yield* withItems({
    pageMatch: new Item({
      label: "page match",
      context: [new Contexts.Page()],
    }),
    anyMatch: new Item({
      label: "any match"
    })
  }, function*({pageMatch, anyMatch}) {
    yield select("b");
    isntPageMatch((yield captureContextMenu("i")),
                  "page context does not match if there is a selection");

    yield select(null);
    isPageMatch((yield captureContextMenu("i")),
                "page context match if there is no selection");
  });
}, data`<body><i>one</i><b>two</b></body>`);

exports["test selection context"] = withTab(function*(assert) {
  yield* withItems({
    item: new Item({
      label: "selection",
      context: [new Contexts.Selection()]
    })
  }, function*({item}) {
    assert.deepEqual((yield captureContextMenu()),
                     menugroup(),
                     "item does not match if there is no selection");

    yield select("b");

    assert.deepEqual((yield captureContextMenu()),
                     menugroup(menuseparator(),
                               menuitem({label: "selection"})),
                     "item matches if there is a selection");
  });
}, data`<i>one</i><b>two</b>`);

exports["test selection context in textarea"] = withTab(function*(assert) {
  yield* withItems({
    item: new Item({
      label: "selection",
      context: [new Contexts.Selection()]
    })
  }, function*({item}) {
    assert.deepEqual((yield captureContextMenu()),
                     menugroup(),
                     "does not match if there's no selection");

    yield select({target:"textarea", start:0, end:5});

    assert.deepEqual((yield captureContextMenu("b")),
                 menugroup(),
                 "does not match if target isn't input with selection");

    assert.deepEqual((yield captureContextMenu("textarea")),
                     menugroup(menuseparator(),
                               menuitem({label: "selection"})),
                     "matches if target is input with selected text");

    yield select({target: "textarea", start: 0, end: 0});

    assert.deepEqual((yield captureContextMenu("textarea")),
                 menugroup(),
                 "does not match when selection is cleared");
  });
}, data`<textarea>Hello World</textarea><b>!!</b>`);

exports["test url contexts"] = withTab(function*(assert) {
  yield* withItems({
    a: new Item({
      label: "a",
      context: [new Contexts.URL(testPageURI)]
    }),
    b: new Item({
      label: "b",
      context: [new Contexts.URL("*.bogus.com")]
    }),
    c: new Item({
      label: "c",
      context: [new Contexts.URL("*.bogus.com"),
                new Contexts.URL(testPageURI)]
    }),
    d: new Item({
      label: "d",
      context: [new Contexts.URL(/.*\.html/)]
    }),
    e: new Item({
      label: "e",
      context: [new Contexts.URL("http://*"),
                new Contexts.URL(testPageURI)]
    }),
    f: new Item({
      label: "f",
      context: [new Contexts.URL("http://*").required,
                new Contexts.URL(testPageURI)]
    }),
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu()),
                     menugroup(menuseparator(),
                               menuitem({label: "a"}),
                               menuitem({label: "c"}),
                               menuitem({label: "d"}),
                               menuitem({label: "e"})),
                     "shows only matching items");
  });
}, testPageURI);

exports["test iframe context"] = withTab(function*(assert) {
  yield* withItems({
    page: new Item({
      label: "page",
      context: [new Contexts.Page()]
    }),
    iframe: new Item({
      label: "iframe",
      context: [new Contexts.Frame()]
    }),
    h2: new Item({
      label: "element",
      context: [new Contexts.Selector("*")]
    })
  }, function(_) {
    assert.deepEqual((yield captureContextMenu("iframe")),
                     menugroup(menuseparator(),
                               menuitem({label: "page"}),
                               menuitem({label: "iframe"}),
                               menuitem({label: "element"})),
                     "matching items are present");

    assert.deepEqual((yield captureContextMenu("h1")),
                     menugroup(menuseparator(),
                               menuitem({label: "page"}),
                               menuitem({label: "element"})),
                     "only matching items are present");

  });

},
data`<h1>hello</h1>
<iframe src='data:text/html,<body>Bye</body>' />`);

exports["test link context"] = withTab(function*(assert) {
  yield* withItems({
    item: new Item({
      label: "link",
      context: [new Contexts.Link()]
    })
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("h1")),
                     menugroup(menuseparator(),
                               menuitem({label: "link"})),
                     "matches anchor child");

    assert.deepEqual((yield captureContextMenu("i")),
                     menugroup(menuseparator(),
                               menuitem({label: "link"})),
                     "matches anchor decedent");
    assert.deepEqual((yield captureContextMenu("h2")),
                     menugroup(),
                     "does not match if not under anchor");
  });
}, data`<a href="/link"><h1>Hello <i>World</i></h1></a><h2>miss</h2>`);


exports["test editable context"] = withTab(function*(assert) {
  const isntEditable = function*(selector) {
    assert.deepEqual((yield captureContextMenu(selector)),
                     menugroup(),
                     `${selector} isn't editable`);
  };

  const isEditable = function*(selector) {
    assert.deepEqual((yield captureContextMenu(selector)),
                     menugroup(menuseparator(),
                               menuitem({label: "editable"})),
                     `${selector} is editable`);
  };

  yield* withItems({
    item: new Item({
      label: "editable",
      context: [new Contexts.Editable()]
    })
  }, function*(_) {
    yield* isntEditable("h1");
    yield* isEditable("input[id=text]");
    yield* isntEditable("input[disabled=true]");
    yield* isntEditable("input[readonly=true]");
    yield* isntEditable("input[type=submit]");
    yield* isntEditable("input[type=radio]");
    yield* isntEditable("input[type=checkbox]");
    yield* isEditable("input[type=foo]");
    yield* isEditable("textarea");
    yield* isEditable("[contenteditable=true]");
  });
}, data`<body>
<h1>examles</h1>
<pre contenteditable="true">This content is editable.</pre>
<input type="text" readonly="true" value="readonly value">
<input type="text" disabled="true" value="disabled value">
<input type="text" id=text value="test value">
<input type="submit" />
<input type="radio" />
<input type="foo" />
<input type="checkbox" />
<textarea>A text field,
with some text.</textarea>
</body>`);

exports["test image context"] = withTab(function*(assert) {
  yield withItems({
    item: new Item({
      label: "image",
      context: [new Contexts.Image()]
    })
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("img")),
                     menugroup(menuseparator(), menuitem({label: "image"})),
                     `<img/> matches image context`);

    assert.deepEqual((yield captureContextMenu("p image")),
                     menugroup(),
                     `<image/> does not image context`);

    assert.deepEqual((yield captureContextMenu("svg image")),
                     menugroup(menuseparator(), menuitem({label: "image"})),
                     `<svg:image/> matches image context`);
  });
}, data`<body>
<p><image style="width: 50px; height: 50px" /></p>
<img src='' />
<div>
<svg xmlns="http://www.w3.org/2000/svg"
     xmlns:xlink= "http://www.w3.org/1999/xlink">
  <image x="0" y="0" height="50px" width="50px" xlink:href=""/>
</svg>
<div>
</body>`);


exports["test audiot & video contexts"] = withTab(function*(assert) {
  yield withItems({
    audio: new Item({
      label: "audio",
      context: [new Contexts.Audio()]
    }),
    video: new Item({
      label: "video",
      context: [new Contexts.Video()]
    }),
    media: new Item({
      label: "media",
      context: [new Contexts.Audio(),
                new Contexts.Video()]
    })
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("img")),
                     menugroup(),
                     `<img/> does not match video or audio context`);

    assert.deepEqual((yield captureContextMenu("audio")),
                     menugroup(menuseparator(),
                               menuitem({label: "audio"}),
                               menuitem({label: "media"})),
                     `<audio/> matches audio context`);

    assert.deepEqual((yield captureContextMenu("video")),
                     menugroup(menuseparator(),
                               menuitem({label: "video"}),
                               menuitem({label: "media"})),
                     `<video/> matches video context`);
  })
}, data`<body>
<div><video width=10 height=10 controls /></div>
<div><audio width=10 height=10 controls /></div>
<div><image style="width: 50px; height: 50px" /></div>
</body>`);

const predicateTestURL = data`<html>
  <head>
    <style>
      p, object, embed { display: inline-block; }
    </style>
  </head>
  <body>
    <strong><p>paragraph</p></strong>
    <p><a href=./link><span>link</span></a></p>
    <p><h3>hi</h3></p>
    <p><button>button</button></p>
    <p><canvas height=50 width=50 /></p>
    <p><img height=50 width=50 src="./no.png" /></p>
    <p><code contenteditable="true">This content is editable.</code></p>
    <p><input type="text" readonly="true" value="readonly value"></p>
    <p><input type="text" disabled="true" value="disabled value"></p>
    <p><input type="text" id=text value="test value" /></p>
    <p><input type="submit" /></p>
    <p><input type="radio" /></p>
    <p><input type="foo" /></p>
    <p><input type="checkbox" /></p>
    <p><textarea>A text field,
    with some text.</textarea></p>
    <p><iframe src='data:text/html,<body style="height:100%">Bye</body>'></iframe></p>
    <p><select><option>one</option><option>two</option></select></p>
    <p><menu><button>item</button></menu></p>
    <p><object width=10 height=10><param name=foo value=bar /></object></p>
    <p><embed width=10 height=10/></p>
    <p><video width=50 height=50 controls /></p>
    <p><audio width=10 height=10 controls /></p>
    <p><applet width=30 height=30 /></p>
  </body>
</html>`;
exports["test predicate context"] = withTab(function*(assert) {
  const test = function*(selector, expect) {
    var isMatch = false;
    test.return = (target) => {
      return isMatch = expect(target);
    }
    assert.deepEqual((yield captureContextMenu(selector)),
                     isMatch ? menugroup(menuseparator(),
                                         menuitem({label:"predicate"})) :
                               menugroup(),
                     isMatch ? `predicate item matches ${selector}` :
                     `predicate item doesn't match ${selector}`);
  };
  test.predicate = target => test.return(target);

  yield* withItems({
    item: new Item({
      label: "predicate",
      read: {
        mediaType: new Readers.MediaType(),
        link: new Readers.LinkURL(),
        isPage: new Readers.isPage(),
        isFrame: new Readers.isFrame(),
        isEditable: new Readers.isEditable(),
        tagName: new Readers.Query("tagName"),
        appCodeName: new Readers.Query("ownerDocument.defaultView.navigator.appCodeName"),
        width: new Readers.Attribute("width"),
        src: new Readers.SrcURL(),
        url: new Readers.PageURL(),
        selection: new Readers.Selection()
      },
      context: [Contexts.Predicate(test.predicate)]
    })
  }, function*(items) {
    yield* test("strong p", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: true,
        isFrame: false,
        isEditable: false,
        tagName: "P",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "pagraph read test");
      return true;
    });

    yield* test("a span", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: "./link",
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "SPAN",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "video tag test");
      return false;
    });

    yield* test("h3", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: true,
        isFrame: false,
        isEditable: false,
        tagName: "H3",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "video tag test");
      return false;
    });

    yield select("h3");

    yield* test("a span", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: "./link",
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "SPAN",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: "hi",
      }, "test selection with link");
      return true;
    });

    yield select(null);


    yield* test("button", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "BUTTON",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test button");
      return true;
    });

    yield* test("canvas", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "CANVAS",
        appCodeName: "Mozilla",
        width: "50",
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test button");
      return true;
    });

    yield* test("img", target => {
      assert.deepEqual(target, {
        mediaType: "image",
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "IMG",
        appCodeName: "Mozilla",
        width: "50",
        src: "./no.png",
        url: predicateTestURL,
        selection: null,
      }, "test image");
      return true;
    });

    yield* test("code", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: true,
        tagName: "CODE",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test content editable");
      return false;
    });

    yield* test("input[readonly=true]", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test readonly input");
      return false;
    });

    yield* test("input[disabled=true]", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test disabled input");
      return false;
    });

    yield select({target: "input#text", start: 0, end: 5 });

    yield* test("input#text", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: true,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: "test ",
      }, "test editable input");
      return false;
    });

    yield select({target: "input#text", start:0, end: 0});

    yield* test("input[type=submit]", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test submit input");
      return false;
    });

    yield* test("input[type=radio]", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test radio input");
      return false;
    });

    yield* test("input[type=checkbox]", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test checkbox input");
      return false;
    });

    yield* test("input[type=foo]", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: true,
        tagName: "INPUT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test unrecognized input");
      return false;
    });

    yield* test("textarea", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: true,
        tagName: "TEXTAREA",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test textarea");
      return false;
    });


    yield* test("iframe", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: true,
        isFrame: true,
        isEditable: false,
        tagName: "BODY",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: `data:text/html,<body%20style="height:100%">Bye</body>`,
        selection: null,
      }, "test iframe");
      return true;
    });

    yield* test("select", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "SELECT",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test select");
      return true;
    });

    yield* test("menu", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "MENU",
        appCodeName: "Mozilla",
        width: null,
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test menu");
      return false;
    });

    yield* test("video", target => {
      assert.deepEqual(target, {
        mediaType: "video",
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "VIDEO",
        appCodeName: "Mozilla",
        width: "50",
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test video");
      return true;
    });

    yield* test("audio", target => {
      assert.deepEqual(target, {
        mediaType: "audio",
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "AUDIO",
        appCodeName: "Mozilla",
        width: "10",
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test audio");
      return true;
    });

    yield* test("object", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "OBJECT",
        appCodeName: "Mozilla",
        width: "10",
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test object");
      return true;
    });

    yield* test("embed", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "EMBED",
        appCodeName: "Mozilla",
        width: "10",
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test embed");
      return true;
    });

    yield* test("applet", target => {
      assert.deepEqual(target, {
        mediaType: null,
        link: null,
        isPage: false,
        isFrame: false,
        isEditable: false,
        tagName: "APPLET",
        appCodeName: "Mozilla",
        width: "30",
        src: null,
        url: predicateTestURL,
        selection: null,
      }, "test applet");
      return false;
    });

  });
}, predicateTestURL);

exports["test extractor reader"] = withTab(function*(assert) {
  const test = function*(selector, expect) {
    var isMatch = false;
    test.return = (target) => {
      return isMatch = expect(target);
    }
    assert.deepEqual((yield captureContextMenu(selector)),
                     isMatch ? menugroup(menuseparator(),
                                         menuitem({label:"extractor"})) :
                               menugroup(),
                     isMatch ? `predicate item matches ${selector}` :
                     `predicate item doesn't match ${selector}`);
  };
  test.predicate = target => test.return(target);


  yield* withItems({
    item: new Item({
      label: "extractor",
      context: [Contexts.Predicate(test.predicate)],
      read: {
        tagName: Readers.Query("tagName"),
        selector: Readers.Extractor(target => {
          let node = target;
          let path = [];
          while (node) {
            if (node.id) {
              path.unshift(`#${node.id}`);
              node = null;
            }
            else {
              path.unshift(node.localName);
              node = node.parentElement;
            }
          }
          return path.join(" > ");
        })
      }
    })
  }, function*(_) {
    yield* test("footer", target => {
      assert.deepEqual(target, {
        tagName: "FOOTER",
        selector: "html > body > nav > footer"
      }, "test footer");
      return false;
    });


  });
}, data`<html>
  <body>
    <nav>
      <header>begin</header>
      <footer>end</footer>
    </nav>
    <article data-index=1>
      <header>First title</header>
      <div>
        <p>First paragraph</p>
        <p>Second paragraph</p>
      </div>
    </article>
    <article data-index=2>
      <header>Second title</header>
      <div>
        <p>First <strong id=foo>paragraph</strong></p>
        <p>Second paragraph</p>
      </div>
    </article>
  </body>
</html>`);

exports["test items overflow"] = withTab(function*(assert) {
  yield* withItems({
    i1: new Item({label: "item-1"}),
    i2: new Item({label: "item-2"}),
    i3: new Item({label: "item-3"}),
    i4: new Item({label: "item-4"}),
    i5: new Item({label: "item-5"}),
    i6: new Item({label: "item-6"}),
    i7: new Item({label: "item-7"}),
    i8: new Item({label: "item-8"}),
    i9: new Item({label: "item-9"}),
    i10: new Item({label: "item-10"}),
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("p")),
                     menugroup(menu({
                                className: "sdk-context-menu-overflow-menu",
                                label: "Add-ons",
                                accesskey: "A",
                              }, menuitem({label: "item-1"}),
                                 menuitem({label: "item-2"}),
                                 menuitem({label: "item-3"}),
                                 menuitem({label: "item-4"}),
                                 menuitem({label: "item-5"}),
                                 menuitem({label: "item-6"}),
                                 menuitem({label: "item-7"}),
                                 menuitem({label: "item-8"}),
                                 menuitem({label: "item-9"}),
                                 menuitem({label: "item-10"}))),
                     "context menu has an overflow");
  });

  prefs.set("extensions.addon-sdk.context-menu.overflowThreshold", 3);

  yield* withItems({
    i1: new Item({label: "item-1"}),
    i2: new Item({label: "item-2"}),
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("p")),
                     menugroup(menuseparator(),
                               menuitem({label: "item-1"}),
                               menuitem({label: "item-2"})),
                     "two items do not overflow");
  });

  yield* withItems({
    one: new Item({label: "one"}),
    two: new Item({label: "two"}),
    three: new Item({label: "three"})
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("p")),
                     menugroup(menu({className: "sdk-context-menu-overflow-menu",
                                     label: "Add-ons",
                                     accesskey: "A"},
                                     menuitem({label: "one"}),
                                     menuitem({label: "two"}),
                                     menuitem({label: "three"}))),
                     "three items overflow");
  });

  prefs.reset("extensions.addon-sdk.context-menu.overflowThreshold");

  yield* withItems({
    one: new Item({label: "one"}),
    two: new Item({label: "two"}),
    three: new Item({label: "three"})
  }, function*(_) {
    assert.deepEqual((yield captureContextMenu("p")),
                     menugroup(menuseparator(),
                               menuitem({label: "one"}),
                               menuitem({label: "two"}),
                               menuitem({label: "three"})),
                     "three items no longer overflow");
  });
}, data`<p>Hello</p>`);


exports["test context menus"] = withTab(function*(assert) {
  const one = new Item({
    label: "one",
    context: [Contexts.Selector("p")],
    read: {tagName: Readers.Query("tagName")}
  });

  assert.deepEqual((yield captureContextMenu("p")),
                    menugroup(menuseparator(),
                              menuitem({label: "one"})),
                    "item is present");

  const two = new Item({
    label: "two",
    read: {tagName: Readers.Query("tagName")}
  });


  assert.deepEqual((yield captureContextMenu("p")),
                   menugroup(menuseparator(),
                             menuitem({label: "one"}),
                             menuitem({label: "two"})),
                   "both items are present");

  const groupLevel1 = new Menu({label: "Level 1"},
                          [one]);

  assert.deepEqual((yield captureContextMenu("p")),
                   menugroup(menuseparator(),
                             menuitem({label: "two"}),
                             menu({label: "Level 1"},
                                  menuitem({label: "one"}))),
                   "first item moved to group");

  assert.deepEqual((yield captureContextMenu("h1")),
                   menugroup(menuseparator(),
                             menuitem({label: "two"})),
                   "menu is hidden since only item does not match");


  const groupLevel2 = new Menu({label: "Level 2" }, [groupLevel1]);

  assert.deepEqual((yield captureContextMenu("p")),
                   menugroup(menuseparator(),
                             menuitem({label: "two"}),
                             menu({label: "Level 2"},
                                  menu({label: "Level 1"},
                                       menuitem({label: "one"})))),
                   "top level menu moved to submenu");

  assert.deepEqual((yield captureContextMenu("h1")),
                   menugroup(menuseparator(),
                             menuitem({label: "two"})),
                   "menu is hidden since only item does not match");


  const contextGroup = new Menu({
    label: "H1 Group",
    context: [Contexts.Selector("h1")]
  }, [
    two,
    new Separator(),
    new Item({ label: "three" })
  ]);


  assert.deepEqual((yield captureContextMenu("p")),
                   menugroup(menuseparator(),
                             menu({label: "Level 2"},
                                  menu({label: "Level 1"},
                                       menuitem({label: "one"})))),
                   "nested menu is rendered");

  assert.deepEqual((yield captureContextMenu("h1")),
                   menugroup(menuseparator(),
                             menu({label: "H1 Group"},
                                  menuitem({label: "two"}),
                                  menuseparator(),
                                  menuitem({label: "three"}))),
                   "new contextual menu rendered");

  yield* withItems({one, two,
                    groupLevel1, groupLevel2, contextGroup}, function*() {

  });

  assert.deepEqual((yield captureContextMenu("p")),
                   menugroup(),
                   "everyhing matching p was desposed");

  assert.deepEqual((yield captureContextMenu("h1")),
                 menugroup(),
                 "everyhing matching h1 was desposed");

}, data`<body><h1>Title</h1><p>Content</p></body>`);

exports["test unloading"] = withTab(function*(assert) {
  const { Loader } = require("sdk/test/loader");
  const loader = Loader(module);

  const {Item, Menu, Separator, Contexts, Readers } = loader.require("sdk/context-menu@2");

  const item = new Item({label: "item"});
  const group = new Menu({label: "menu"},
                         [new Separator(),
                          new Item({label: "sub-item"})]);
  assert.deepEqual((yield captureContextMenu()),
                   menugroup(menuseparator(),
                             menuitem({label: "item"}),
                             menu({label: "menu"},
                                  menuseparator(),
                                  menuitem({label: "sub-item"}))),
                   "all items rendered");


  loader.unload();

  assert.deepEqual((yield captureContextMenu()),
                 menugroup(),
                 "all items disposed");
}, data`<body></body>`);

if (require("@loader/options").isNative) {
  module.exports = {
    "test skip on jpm": (assert) => assert.pass("skipping this file with jpm")
  };
}

require("sdk/test").run(module.exports);
