/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cc, Ci, Cu } = require("chrome");
const { LoaderWithHookedConsole } = require('sdk/test/loader');
const url = require("sdk/url");
const timer = require("sdk/timers");
const self = require("sdk/self");
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { close, open, focus } = require("sdk/window/helpers");
const tabs = require("sdk/tabs/utils");
const { merge } = require("sdk/util/object");
const unload = require("sdk/system/unload");
const fixtures = require("./fixtures");

let jetpackID = "testID";
try {
  jetpackID = require("sdk/self").id;
} catch(e) {}

function openNewWindowTab(url, options) {
  return open('chrome://browser/content/browser.xul', {
    features: {
      chrome: true,
      toolbar: true
    }
  }).then(focus).then(function(window) {
    if (options.onLoad) {
      options.onLoad({ target: { defaultView: window } })
    }
  });
}

exports.testDeprecationMessage = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module, onMessage);

  // Intercept all console method calls
  let calls = [];
  function onMessage(type, msg) {
    assert.equal(type, 'error', 'the only message is an error');
    assert.ok(/^DEPRECATED:/.test(msg), 'deprecated');
    loader.unload();
    done();
  }
  loader.require('sdk/widget');
}

exports.testConstructor = function(assert, done) {
  let { loader: loader0 } = LoaderWithHookedConsole(module);
  const widgets = loader0.require("sdk/widget");
  let browserWindow = getMostRecentBrowserWindow();
  let doc = browserWindow.document;
  let AddonsMgrListener;

  AddonsMgrListener = {
    onInstalling: () => {},
    onInstalled: () => {},
    onUninstalling: () => {},
    onUninstalled: () => {}
  };

  let container = () => doc.getElementById("nav-bar");
  let getWidgets = () => container() ? container().querySelectorAll('[id^="widget\:"]') : [];
  let widgetCount = () => getWidgets().length;
  let widgetStartCount = widgetCount();
  let widgetNode = (index) => getWidgets()[index];

  // Test basic construct/destroy
  AddonsMgrListener.onInstalling();
  let w = widgets.Widget({ id: "basic-construct-destroy", label: "foo", content: "bar" });
  AddonsMgrListener.onInstalled();
  assert.equal(widgetCount(), widgetStartCount + 1, "panel has correct number of child elements after widget construction");

  // test widget height
  assert.equal(widgetNode(0).firstChild.boxObject.height, 16, "widget has correct default height");

  AddonsMgrListener.onUninstalling();
  w.destroy();
  AddonsMgrListener.onUninstalled();
  w.destroy();
  assert.pass("Multiple destroys do not cause an error");
  assert.equal(widgetCount(), widgetStartCount, "panel has correct number of child elements after destroy");

  // Test automatic widget destroy on unload
  let { loader } = LoaderWithHookedConsole(module);
  let widgetsFromLoader = loader.require("sdk/widget");
  widgetStartCount = widgetCount();
  w = widgetsFromLoader.Widget({ id: "destroy-on-unload", label: "foo", content: "bar" });
  assert.equal(widgetCount(), widgetStartCount + 1, "widget has been correctly added");
  loader.unload();
  assert.equal(widgetCount(), widgetStartCount, "widget has been destroyed on module unload");

  // Test nothing
  assert.throws(
    function() widgets.Widget({}),
    /^The widget must have a non-empty label property\.$/,
    "throws on no properties");

  // Test no label
  assert.throws(
    function() widgets.Widget({content: "foo"}),
    /^The widget must have a non-empty label property\.$/,
    "throws on no label");

  // Test empty label
  assert.throws(
    function() widgets.Widget({label: "", content: "foo"}),
    /^The widget must have a non-empty label property\.$/,
    "throws on empty label");

  // Test no content or image
  assert.throws(
    function() widgets.Widget({id: "no-content-throws", label: "foo"}),
    /^No content or contentURL property found\. Widgets must have one or the other\.$/,
    "throws on no content");

  // Test empty content, no image
  assert.throws(
    function() widgets.Widget({id:"empty-content-throws", label: "foo", content: ""}),
    /^No content or contentURL property found\. Widgets must have one or the other\.$/,
    "throws on empty content");

  // Test empty image, no content
  assert.throws(
    function() widgets.Widget({id:"empty-image-throws", label: "foo", image: ""}),
    /^No content or contentURL property found\. Widgets must have one or the other\.$/,
    "throws on empty content");

  // Test empty content, empty image
  assert.throws(
    function() widgets.Widget({id:"empty-image-and-content-throws", label: "foo", content: "", image: ""}),
    /^No content or contentURL property found. Widgets must have one or the other\.$/,
    "throws on empty content");

  // Test duplicated ID
  let duplicateID = widgets.Widget({id: "foo", label: "foo", content: "bar"});
  assert.throws(
    function() widgets.Widget({id: "foo", label: "bar", content: "bar"}),
    /^This widget ID is already used: foo$/,
    "throws on duplicated id");
  duplicateID.destroy();

  // Test Bug 652527
  assert.throws(
    function() widgets.Widget({id: "", label: "bar", content: "bar"}),
    /^You have to specify a unique value for the id property of your widget in order for the application to remember its position\./,
    "throws on falsey id");

  // Test duplicate label, different ID
  let w1 = widgets.Widget({id: "id1", label: "foo", content: "bar"});
  let w2 = widgets.Widget({id: "id2", label: "foo", content: "bar"});
  w1.destroy();
  w2.destroy();

  // Test position restore on create/destroy/create
  // Create 3 ordered widgets
  w1 = widgets.Widget({id: "position-first", label:"first", content: "bar"});
  w2 = widgets.Widget({id: "position-second", label:"second", content: "bar"});
  let w3 = widgets.Widget({id: "position-third", label:"third", content: "bar"});
  // Remove the middle widget
  assert.equal(widgetNode(1).getAttribute("label"), "second", "second widget is the second widget inserted");
  w2.destroy();
  assert.equal(widgetNode(1).getAttribute("label"), "third", "second widget is removed, so second widget is now the third one");
  w2 = widgets.Widget({id: "position-second", label:"second", content: "bar"});
  assert.equal(widgetNode(1).getAttribute("label"), "second", "second widget is created again, at the same location");
  // Cleanup this testcase
  AddonsMgrListener.onUninstalling();
  w1.destroy();
  w2.destroy();
  w3.destroy();
  AddonsMgrListener.onUninstalled();

  // Helper for testing a single widget.
  // Confirms proper addition and content setup.
  function testSingleWidget(widgetOptions) {
    // We have to display which test is being run, because here we do not
    // use the regular test framework but rather a custom one that iterates
    // the `tests` array.
    assert.pass("executing: " + widgetOptions.id);

    let startCount = widgetCount();
    let widget = widgets.Widget(widgetOptions);
    let node = widgetNode(startCount);
    assert.ok(node, "widget node at index");
    assert.equal(node.tagName, "toolbaritem", "widget element is correct");
    assert.equal(widget.width + "px", node.style.minWidth, "widget width is correct");
    assert.equal(widgetCount(), startCount + 1, "container has correct number of child elements");
    let content = node.firstElementChild;
    assert.ok(content, "found content");
    assert.ok(/iframe|image/.test(content.tagName), "content is iframe or image");
    return widget;
  }

  // Array of widgets to test
  // and a function to test them.
  let tests = [];
  function nextTest() {
    assert.equal(widgetCount(), 0, "widget in last test property cleaned itself up");
    if (!tests.length) {
      loader0.unload();
      done();
    }
    else
      timer.setTimeout(tests.shift(), 0);
  }
  function doneTest() nextTest();

  // text widget
  tests.push(function testTextWidget() testSingleWidget({
    id: "text-single",
    label: "text widget",
    content: "oh yeah",
    contentScript: "self.postMessage(document.body.innerHTML);",
    contentScriptWhen: "end",
    onMessage: function (message) {
      assert.equal(this.content, message, "content matches");
      this.destroy();
      doneTest();
    }
  }));

  // html widget
  tests.push(function testHTMLWidget() testSingleWidget({
    id: "html",
    label: "html widget",
    content: "<div>oh yeah</div>",
    contentScript: "self.postMessage(document.body.innerHTML);",
    contentScriptWhen: "end",
    onMessage: function (message) {
      assert.equal(this.content, message, "content matches");
      this.destroy();
      doneTest();
    }
  }));

  // image url widget
  tests.push(function testImageURLWidget() testSingleWidget({
    id: "image",
    label: "image url widget",
    contentURL: fixtures.url("test.html"),
    contentScript: "self.postMessage({title: document.title, " +
                   "tag: document.body.firstElementChild.tagName, " +
                   "content: document.body.firstElementChild.innerHTML});",
    contentScriptWhen: "end",
    onMessage: function (message) {
      assert.equal(message.title, "foo", "title matches");
      assert.equal(message.tag, "P", "element matches");
      assert.equal(message.content, "bar", "element content matches");
      this.destroy();
      doneTest();
    }
  }));

  // web uri widget
  tests.push(function testWebURIWidget() testSingleWidget({
    id: "web",
    label: "web uri widget",
    contentURL: fixtures.url("test.html"),
    contentScript: "self.postMessage({title: document.title, " +
                   "tag: document.body.firstElementChild.tagName, " +
                   "content: document.body.firstElementChild.innerHTML});",
    contentScriptWhen: "end",
    onMessage: function (message) {
      assert.equal(message.title, "foo", "title matches");
      assert.equal(message.tag, "P", "element matches");
      assert.equal(message.content, "bar", "element content matches");
      this.destroy();
      doneTest();
    }
  }));

  // event: onclick + content
  tests.push(function testOnclickEventContent() testSingleWidget({
    id: "click-content",
    label: "click test widget - content",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('click', {button: 0});" +
                   "document.getElementById('me').dispatchEvent(evt);",
    contentScriptWhen: "end",
    onClick: function() {
      assert.pass("onClick called");
      this.destroy();
      doneTest();
    }
  }));

  // event: onmouseover + content
  tests.push(function testOnmouseoverEventContent() testSingleWidget({
    id: "mouseover-content",
    label: "mouseover test widget - content",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('mouseover'); " +
                   "document.getElementById('me').dispatchEvent(evt);",
    contentScriptWhen: "end",
    onMouseover: function() {
      assert.pass("onMouseover called");
      this.destroy();
      doneTest();
    }
  }));

  // event: onmouseout + content
  tests.push(function testOnmouseoutEventContent() testSingleWidget({
    id: "mouseout-content",
    label: "mouseout test widget - content",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('mouseout');" +
                   "document.getElementById('me').dispatchEvent(evt);",
    contentScriptWhen: "end",
    onMouseout: function() {
      assert.pass("onMouseout called");
      this.destroy();
      doneTest();
    }
  }));

  // event: onclick + image
  tests.push(function testOnclickEventImage() testSingleWidget({
    id: "click-image",
    label: "click test widget - image",
    contentURL: fixtures.url("moz_favicon.ico"),
    contentScript: "var evt = new MouseEvent('click'); " +
                   "document.body.firstElementChild.dispatchEvent(evt);",
    contentScriptWhen: "end",
    onClick: function() {
      assert.pass("onClick called");
      this.destroy();
      doneTest();
    }
  }));

  // event: onmouseover + image
  tests.push(function testOnmouseoverEventImage() testSingleWidget({
    id: "mouseover-image",
    label: "mouseover test widget - image",
    contentURL: fixtures.url("moz_favicon.ico"),
    contentScript: "var evt = new MouseEvent('mouseover');" +
                   "document.body.firstElementChild.dispatchEvent(evt);",
    contentScriptWhen: "end",
    onMouseover: function() {
      assert.pass("onMouseover called");
      this.destroy();
      doneTest();
    }
  }));

  // event: onmouseout + image
  tests.push(function testOnmouseoutEventImage() testSingleWidget({
    id: "mouseout-image",
    label: "mouseout test widget - image",
    contentURL: fixtures.url("moz_favicon.ico"),
    contentScript: "var evt = new MouseEvent('mouseout'); " +
                   "document.body.firstElementChild.dispatchEvent(evt);",
    contentScriptWhen: "end",
    onMouseout: function() {
      assert.pass("onMouseout called");
      this.destroy();
      doneTest();
    }
  }));

  // test multiple widgets
  tests.push(function testMultipleWidgets() {
    let w1 = widgets.Widget({id: "first", label: "first widget", content: "first content"});
    let w2 = widgets.Widget({id: "second", label: "second widget", content: "second content"});

    w1.destroy();
    w2.destroy();

    doneTest();
  });

  // test updating widget content
  let loads = 0;
  tests.push(function testUpdatingWidgetContent() testSingleWidget({
    id: "content-updating",
    label: "content update test widget",
    content: "<div id='me'>foo</div>",
    contentScript: "self.postMessage(1)",
    contentScriptWhen: "ready",
    onMessage: function(message) {
      if (!this.flag) {
        this.content = "<div id='me'>bar</div>";
        this.flag = 1;
      }
      else {
        assert.equal(this.content, "<div id='me'>bar</div>", 'content is as expected');
        this.destroy();
        doneTest();
      }
    }
  }));

  // test updating widget contentURL
  let url1 = "data:text/html;charset=utf-8,<body>foodle</body>";
  let url2 = "data:text/html;charset=utf-8,<body>nistel</body>";

  tests.push(function testUpdatingContentURL() testSingleWidget({
    id: "content-url-updating",
    label: "content update test widget",
    contentURL: url1,
    contentScript: "self.postMessage(document.location.href);",
    contentScriptWhen: "end",
    onMessage: function(message) {
      if (!this.flag) {
        assert.equal(this.contentURL.toString(), url1);
        assert.equal(message, url1);
        this.contentURL = url2;
        this.flag = 1;
      }
      else {
        assert.equal(this.contentURL.toString(), url2);
        assert.equal(message, url2);
        this.destroy();
        doneTest();
      }
    }
  }));

  // test tooltip
  tests.push(function testTooltip() testSingleWidget({
    id: "text-with-tooltip",
    label: "text widget",
    content: "oh yeah",
    tooltip: "foo",
    contentScript: "self.postMessage(1)",
    contentScriptWhen: "ready",
    onMessage: function(message) {
      assert.equal(this.tooltip, "foo", "tooltip matches");
      this.destroy();
      doneTest();
    }
  }));

  // test tooltip fallback to label
  tests.push(function testTooltipFallback() testSingleWidget({
    id: "fallback",
    label: "fallback",
    content: "oh yeah",
    contentScript: "self.postMessage(1)",
    contentScriptWhen: "ready",
    onMessage: function(message) {
      assert.equal(this.tooltip, this.label, "tooltip fallbacks to label");
      this.destroy();
      doneTest();
    }
  }));

  // test updating widget tooltip
  let updated = false;
  tests.push(function testUpdatingTooltip() testSingleWidget({
    id: "tooltip-updating",
    label: "tooltip update test widget",
    tooltip: "foo",
    content: "<div id='me'>foo</div>",
    contentScript: "self.postMessage(1)",
    contentScriptWhen: "ready",
    onMessage: function(message) {
      this.tooltip = "bar";
      assert.equal(this.tooltip, "bar", "tooltip gets updated");
      this.destroy();
      doneTest();
    }
  }));

  // test allow attribute
  tests.push(function testDefaultAllow() testSingleWidget({
    id: "allow-default",
    label: "allow.script attribute",
    content: "<script>document.title = 'ok';</script>",
    contentScript: "self.postMessage(document.title)",
    onMessage: function(message) {
      assert.equal(message, "ok", "scripts are evaluated by default");
      this.destroy();
      doneTest();
    }
  }));

  tests.push(function testExplicitAllow() testSingleWidget({
    id: "allow-explicit",
    label: "allow.script attribute",
    allow: {script: true},
    content: "<script>document.title = 'ok';</script>",
    contentScript: "self.postMessage(document.title)",
    onMessage: function(message) {
      assert.equal(message, "ok", "scripts are evaluated when we want to");
      this.destroy();
      doneTest();
    }
  }));

  tests.push(function testExplicitDisallow() testSingleWidget({
    id: "allow-explicit-disallow",
    label: "allow.script attribute",
    content: "<script>document.title = 'ok';</script>",
    allow: {script: false},
    contentScript: "self.postMessage(document.title)",
    onMessage: function(message) {
      assert.notEqual(message, "ok", "scripts aren't evaluated when " +
                                         "explicitly blocked it");
      this.destroy();
      doneTest();
    }
  }));

  // test multiple windows
  tests.push(function testMultipleWindows() {
    assert.pass('executing test multiple windows');
    openNewWindowTab("about:blank", { inNewWindow: true, onLoad: function(e) {
      let browserWindow = e.target.defaultView;
      assert.ok(browserWindow, 'window was opened');
      let doc = browserWindow.document;
      let container = () => doc.getElementById("nav-bar");
      let widgetCount2 = () => container() ? container().querySelectorAll('[id^="widget\:"]').length : 0;
      let widgetStartCount2 = widgetCount2();

      let w1Opts = {id:"first-multi-window", label: "first widget", content: "first content"};
      let w1 = testSingleWidget(w1Opts);
      assert.equal(widgetCount2(), widgetStartCount2 + 1, "2nd window has correct number of child elements after first widget");

      let w2Opts = {id:"second-multi-window", label: "second widget", content: "second content"};
      let w2 = testSingleWidget(w2Opts);
      assert.equal(widgetCount2(), widgetStartCount2 + 2, "2nd window has correct number of child elements after second widget");

      w1.destroy();
      assert.equal(widgetCount2(), widgetStartCount2 + 1, "2nd window has correct number of child elements after first destroy");
      w2.destroy();
      assert.equal(widgetCount2(), widgetStartCount2, "2nd window has correct number of child elements after second destroy");

      close(browserWindow).then(doneTest);
    }}).catch(assert.fail);
  });

  // test window closing
  tests.push(function testWindowClosing() {
    // 1/ Create a new widget
    let w1Opts = {
      id:"first-win-closing",
      label: "first widget",
      content: "first content",
      contentScript: "self.port.on('event', function () self.port.emit('event'))"
    };
    let widget = testSingleWidget(w1Opts);
    let windows = loader0.require("sdk/windows").browserWindows;

    // 2/ Retrieve a WidgetView for the initial browser window
    let acceptDetach = false;
    let mainView = widget.getView(windows.activeWindow);
    assert.ok(mainView, "Got first widget view");
    mainView.on("detach", function () {
      // 8/ End of our test. Accept detach event only when it occurs after
      // widget.destroy()
      if (acceptDetach)
        doneTest();
      else
        assert.fail("View on initial window should not be destroyed");
    });
    mainView.port.on("event", function () {
      // 7/ Receive event sent during 6/ and cleanup our test
      acceptDetach = true;
      widget.destroy();
    });

    // 3/ First: open a new browser window
    windows.open({
      url: "about:blank",
      onOpen: function(window) {
        // 4/ Retrieve a WidgetView for this new window
        let view = widget.getView(window);
        assert.ok(view, "Got second widget view");
        view.port.on("event", function () {
          assert.fail("We should not receive event on the detach view");
        });
        view.on("detach", function () {
          // The related view is destroyed
          // 6/ Send a custom event
          assert.throws(function () {
              view.port.emit("event");
            },
            /^The widget has been destroyed and can no longer be used.$/,
            "emit on a destroyed view should throw");
          widget.port.emit("event");
        });

        // 5/ Destroy this window
        window.close();
      }
    });
  });

  if (false) {
    tests.push(function testAddonBarHide() {
      // Hide the addon-bar
      browserWindow.setToolbarVisibility(container(), false);
      assert.ok(container().collapsed,
                "1st window starts with an hidden addon-bar");

      // Then open a browser window and verify that the addon-bar remains hidden
      openNewWindowTab("about:blank", { inNewWindow: true, onLoad: function(e) {
        let browserWindow2 = e.target.defaultView;
        let doc2 = browserWindow2.document;
        function container2() doc2.getElementById("addon-bar");
        function widgetCount2() container2() ? container2().childNodes.length : 0;
        let widgetStartCount2 = widgetCount2();
        assert.ok(container2().collapsed,
                  "2nd window starts with an hidden addon-bar");

        let w1Opts = {id:"first-addonbar-hide", label: "first widget", content: "first content"};
        let w1 = testSingleWidget(w1Opts);
        assert.equal(widgetCount2(), widgetStartCount2 + 1,
                     "2nd window has correct number of child elements after" +
                     "widget creation");
        assert.ok(!container().collapsed, "1st window has a visible addon-bar");
        assert.ok(!container2().collapsed, "2nd window has a visible addon-bar");
        w1.destroy();
        assert.equal(widgetCount2(), widgetStartCount2,
                     "2nd window has correct number of child elements after" +
                     "widget destroy");

        assert.ok(container().collapsed, "1st window has an hidden addon-bar");
        assert.ok(container2().collapsed, "2nd window has an hidden addon-bar");

        // Reset addon-bar visibility before exiting this test
        browserWindow.setToolbarVisibility(container(), true);

        close(browserWindow2).then(doneTest);
      }}).catch(assert.fail);
    });
  }

  // test widget.width
  tests.push(function testWidgetWidth() testSingleWidget({
    id: "text-test-width",
    label: "test widget.width",
    content: "test width",
    width: 64,
    contentScript: "self.postMessage(1)",
    contentScriptWhen: "ready",
    onMessage: function(message) {
      assert.equal(this.width, 64, 'width is 64');

      let node = widgetNode(0);
      assert.equal(this.width, node.style.minWidth.replace("px", ""));
      assert.equal(this.width, node.firstElementChild.style.width.replace("px", ""));
      this.width = 48;
      assert.equal(this.width, node.style.minWidth.replace("px", ""));
      assert.equal(this.width, node.firstElementChild.style.width.replace("px", ""));

      this.destroy();
      doneTest();
    }
  }));

  // test click handler not respond to right-click
  let clickCount = 0;
  tests.push(function testNoRightClick() testSingleWidget({
    id: "right-click-content",
    label: "click test widget - content",
    content: "<div id='me'>foo</div>",
    contentScript: // Left click
                   "var evt = new MouseEvent('click', {button: 0});" +
                   "document.getElementById('me').dispatchEvent(evt); " +
                   // Middle click
                   "evt = new MouseEvent('click', {button: 1});" +
                   "document.getElementById('me').dispatchEvent(evt); " +
                   // Right click
                   "evt = new MouseEvent('click', {button: 2});" +
                   "document.getElementById('me').dispatchEvent(evt); " +
                   // Mouseover
                   "evt = new MouseEvent('mouseover');" +
                   "document.getElementById('me').dispatchEvent(evt);",
    contentScriptWhen: "end",
    onClick: function() clickCount++,
    onMouseover: function() {
      assert.equal(clickCount, 1, "only left click was sent to click handler");
      this.destroy();
      doneTest();
    }
  }));

  // kick off test execution
  doneTest();
};

exports.testWidgetWithValidPanel = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");
  const { Panel } = loader.require("sdk/panel");

  let widget1 = Widget({
    id: "testWidgetWithValidPanel",
    label: "panel widget 1",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('click', {button: 0});" +
                   "document.body.dispatchEvent(evt);",
    contentScriptWhen: "end",
    panel: Panel({
      contentURL: "data:text/html;charset=utf-8,<body>Look ma, a panel!</body>",
      onShow: function() {
        let { document } = getMostRecentBrowserWindow();
        let widgetEle = document.getElementById("widget:" + jetpackID + "-" + widget1.id);
        let panelEle = document.getElementById('mainPopupSet').lastChild;
        // See bug https://bugzilla.mozilla.org/show_bug.cgi?id=859592
        assert.equal(panelEle.getAttribute("type"), "arrow", 'the panel is a arrow type');
        assert.strictEqual(panelEle.anchorNode, widgetEle, 'the panel is properly anchored to the widget');

        widget1.destroy();
        loader.unload();
        assert.pass("panel displayed on click");
        done();
      }
    })
  });
};

exports.testWidgetWithInvalidPanel = function(assert) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");

  assert.throws(
    function() {
      widgets.Widget({
        id: "panel2",
        label: "panel widget 2",
        panel: {}
      });
    },
    /^The option \"panel\" must be one of the following types: null, undefined, object$/,
    "widget.panel must be a Panel object");
  loader.unload();
};

exports.testPanelWidget3 = function testPanelWidget3(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");
  const { Panel } = loader.require("sdk/panel");

  let onClickCalled = false;
  let widget3 = widgets.Widget({
    id: "panel3",
    label: "panel widget 3",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('click', {button: 0});" +
                   "document.body.firstElementChild.dispatchEvent(evt);",
    contentScriptWhen: "end",
    onClick: function() {
      onClickCalled = true;
      this.panel.show();
    },
    panel: Panel({
      contentURL: "data:text/html;charset=utf-8,<body>Look ma, a panel!</body>",
      onShow: function() {
        assert.ok(
          onClickCalled,
          "onClick called on click for widget with both panel and onClick");
        widget3.destroy();
        loader.unload();
        done();
      }
    })
  });
};

exports.testWidgetWithPanelInMenuPanel = function(assert, done) {
  const { CustomizableUI } = Cu.import("resource:///modules/CustomizableUI.jsm", {});
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");
  const { Panel } = loader.require("sdk/panel");

  let widget1 = widgets.Widget({
    id: "panel1",
    label: "panel widget 1",
    content: "<div id='me'>foo</div>",
    contentScript: "new " + function() {
      self.port.on('click', () => {
        let evt = new MouseEvent('click', {button: 0});
        document.body.dispatchEvent(evt);
      });
    },
    contentScriptWhen: "end",
    panel: Panel({
      contentURL: "data:text/html;charset=utf-8,<body>Look ma, a panel!</body>",
      onShow: function() {
        let { document } = getMostRecentBrowserWindow();
        let { anchorNode } = document.getElementById('mainPopupSet').lastChild;
        let panelButtonNode = document.getElementById("PanelUI-menu-button");

        assert.strictEqual(anchorNode, panelButtonNode,
          'the panel is anchored to the panel menu button instead of widget');

        widget1.destroy();
        loader.unload();
        done();
      }
    })
  });

  let widgetId = "widget:" + jetpackID + "-" + widget1.id;

  CustomizableUI.addListener({
    onWidgetAdded: function(id) {
      if (id !== widgetId) return;

      let { document, PanelUI } = getMostRecentBrowserWindow();

      PanelUI.panel.addEventListener('popupshowing', function onshow({type}) {
        this.removeEventListener(type, onshow);
        widget1.port.emit('click');
      });

      document.getElementById("PanelUI-menu-button").click()
    }
  });

  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_PANEL);
};

exports.testWidgetMessaging = function testWidgetMessaging(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");

  let origMessage = "foo";
  let widget = widgets.Widget({
    id: "widget-messaging",
    label: "foo",
    content: "<bar>baz</bar>",
    contentScriptWhen: "end",
    contentScript: "self.on('message', function(data) { self.postMessage(data); }); self.postMessage('ready');",
    onMessage: function(message) {
      if (message == "ready")
        widget.postMessage(origMessage);
      else {
        assert.equal(origMessage, message);
        widget.destroy();
        loader.unload();
        done();
      }
    }
  });
};

exports.testWidgetViews = function testWidgetViews(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");

  let widget = widgets.Widget({
    id: "widget-views",
    label: "foo",
    content: "<bar>baz</bar>",
    contentScriptWhen: "ready",
    contentScript: "self.on('message', function(data) self.postMessage(data)); self.postMessage('ready')",
    onAttach: function(view) {
      assert.pass("WidgetView created");
      view.on("message", function () {
        assert.pass("Got message in WidgetView");
        widget.destroy();
      });
      view.on("detach", function () {
        assert.pass("WidgetView destroyed");
        loader.unload();
        done();
      });
    }
  });
};

exports.testWidgetViewsUIEvents = function testWidgetViewsUIEvents(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");
  const { browserWindows } = loader.require("sdk/windows");

  let view = null;
  let widget = widgets.Widget({
    id: "widget-view-ui-events",
    label: "foo",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('click', {button: 0});" +
                   "document.getElementById('me').dispatchEvent(evt);",
    contentScriptWhen: "ready",
    onAttach: function(attachView) {
      view = attachView;
      assert.pass("Got attach event");
    },
    onClick: function (eventView) {
      assert.equal(view, eventView,
                         "event first argument is equal to the WidgetView");
      let view2 = widget.getView(browserWindows.activeWindow);
      assert.equal(view, view2,
                         "widget.getView return the same WidgetView");
      widget.destroy();
      loader.unload();
      done();
    }
  });
};

exports.testWidgetViewsCustomEvents = function testWidgetViewsCustomEvents(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");

  let widget = widgets.Widget({
    id: "widget-view-custom-events",
    label: "foo",
    content: "<div id='me'>foo</div>",
    contentScript: "self.port.emit('event', 'ok');",
    contentScriptWhen: "ready",
    onAttach: function(view) {
      view.port.on("event", function (data) {
        assert.equal(data, "ok",
                         "event argument is valid on WidgetView");
      });
    },
  });
  widget.port.on("event", function (data) {
    assert.equal(data, "ok", "event argument is valid on Widget");
    widget.destroy();
    loader.unload();
    done();
  });
};

exports.testWidgetViewsTooltip = function testWidgetViewsTooltip(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");
  const { browserWindows } = loader.require("sdk/windows");

  let widget = new widgets.Widget({
    id: "widget-views-tooltip",
    label: "foo",
    content: "foo"
  });
  let view = widget.getView(browserWindows.activeWindow);
  widget.tooltip = null;
  assert.equal(view.tooltip, "foo",
               "view tooltip defaults to base widget label");
  assert.equal(widget.tooltip, "foo",
               "tooltip defaults to base widget label");
  widget.destroy();
  loader.unload();
  done();
};

exports.testWidgetMove = function testWidgetMove(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");

  let browserWindow = getMostRecentBrowserWindow();
  let doc = browserWindow.document;

  let label = "unique-widget-label";
  let origMessage = "message after node move";
  let gotFirstReady = false;

  let widget = widgets.Widget({
    id: "widget-move",
    label: label,
    content: "<bar>baz</bar>",
    contentScriptWhen: "ready",
    contentScript: "self.on('message', function(data) { self.postMessage(data); }); self.postMessage('ready');",
    onMessage: function(message) {
      if (message == "ready") {
        if (!gotFirstReady) {
          assert.pass("Got first ready event");
          let widgetNode = doc.querySelector('toolbaritem[label="' + label + '"]');
          let parent = widgetNode.parentNode;
          parent.insertBefore(widgetNode, parent.firstChild);
          gotFirstReady = true;
        }
        else {
          assert.pass("Got second ready event");
          widget.postMessage(origMessage);
        }
      }
      else {
        assert.equal(origMessage, message, "Got message after node move");
        widget.destroy();
        loader.unload();
        done();
      }
    }
  });
};

/*
The bug is exhibited when a widget with HTML content has it's content
changed to new HTML content with a pound in it. Because the src of HTML
content is converted to a data URI, the underlying iframe doesn't
consider the content change a navigation change, so doesn't load
the new content.
*/
exports.testWidgetWithPound = function testWidgetWithPound(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");

  function getWidgetContent(widget) {
    let browserWindow = getMostRecentBrowserWindow();
    let doc = browserWindow.document;
    let widgetNode = doc.querySelector('toolbaritem[label="' + widget.label + '"]');
    assert.ok(widgetNode, 'found widget node in the front-end');
    return widgetNode.firstChild.contentDocument.body.innerHTML;
  }

  let count = 0;
  let widget = widgets.Widget({
    id: "1",
    label: "foo",
    content: "foo",
    contentScript: "window.addEventListener('load', self.postMessage, false);",
    onMessage: function() {
      count++;
      if (count == 1) {
        widget.content = "foo#";
      }
      else {
        assert.equal(getWidgetContent(widget), "foo#", "content updated to pound?");
        widget.destroy();
        loader.unload();
        done();
      }
    }
  });
};

exports.testContentScriptOptionsOption = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");

  let widget = Widget({
      id: "widget-script-options",
      label: "fooz",
      content: "fooz",
      contentScript: "self.postMessage( [typeof self.options.d, self.options] );",
      contentScriptWhen: "end",
      contentScriptOptions: {a: true, b: [1,2,3], c: "string", d: function(){ return 'test'}},
      onMessage: function(msg) {
        assert.equal( msg[0], 'undefined', 'functions are stripped from contentScriptOptions' );
        assert.equal( typeof msg[1], 'object', 'object as contentScriptOptions' );
        assert.equal( msg[1].a, true, 'boolean in contentScriptOptions' );
        assert.equal( msg[1].b.join(), '1,2,3', 'array and numbers in contentScriptOptions' );
        assert.equal( msg[1].c, 'string', 'string in contentScriptOptions' );
        widget.destroy();
        loader.unload();
        done();
      }
    });
};

exports.testOnAttachWithoutContentScript = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");

  let widget = Widget({
      id: "onAttachNoCS",
      label: "onAttachNoCS",
      content: "onAttachNoCS",
      onAttach: function (view) {
        assert.pass("received attach event");
        widget.destroy();
        loader.unload();
        done();
      }
    });
};

exports.testPostMessageOnAttach = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");

  let widget = Widget({
      id: "onAttach",
      label: "onAttach",
      content: "onAttach",
      // 1) Send a message immediatly after `attach` event
      onAttach: function (view) {
        view.postMessage("ok");
      },
      // 2) Listen to it and forward it back to the widget
      contentScript: "self.on('message', self.postMessage);",
      // 3) Listen to this forwarded message
      onMessage: function (msg) {
        assert.equal( msg, "ok", "postMessage works on `attach` event");
        widget.destroy();
        loader.unload();
        done();
      }
    });
};

exports.testPostMessageOnLocationChange = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");

  let attachEventCount = 0;
  let messagesCount = 0;
  let widget = Widget({
      id: "onLocationChange",
      label: "onLocationChange",
      content: "onLocationChange",
      contentScript: "new " + function ContentScriptScope() {
        // Emit an event when content script is applied in order to know when
        // the first document is loaded so that we can load the 2nd one
        self.postMessage("ready");
        // And forward any incoming message back to the widget to see if
        // messaging is working on 2nd document
        self.on("message", self.postMessage);
      },
      onMessage: function (msg) {
        messagesCount++;
        if (messagesCount == 1) {
          assert.equal(msg, "ready", "First document is loaded");
          widget.content = "location changed";
        }
        else if (messagesCount == 2) {
          assert.equal(msg, "ready", "Second document is loaded");
          widget.postMessage("ok");
        }
        else if (messagesCount == 3) {
          assert.equal(msg, "ok",
                       "We receive the message sent to the 2nd document");
          widget.destroy();
          loader.unload();
          done();
        }
      }
    });
};

exports.testSVGWidget = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");

  // use of capital SVG here is intended, that was failing..
  let SVG_URL = fixtures.url("mofo_logo.SVG");

  let widget = Widget({
    id: "mozilla-svg-logo",
    label: "moz foundation logo",
    contentURL: SVG_URL,
    contentScript: "self.postMessage({count: window.document.images.length, src: window.document.images[0].src});",
    onMessage: function(data) {
      widget.destroy();
      assert.equal(data.count, 1, 'only one image');
      assert.equal(data.src, SVG_URL, 'only one image');
      loader.unload();
      done();
    }
  });
};

exports.testReinsertion = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module);
  const { Widget } = loader.require("sdk/widget");
  const WIDGETID = "test-reinsertion";
  let browserWindow = getMostRecentBrowserWindow();

  let widget = Widget({
    id: "test-reinsertion",
    label: "test reinsertion",
    content: "Test",
  });
  let realWidgetId = "widget:" + jetpackID + "-" + WIDGETID;
  // Remove the widget:

  browserWindow.CustomizableUI.removeWidgetFromArea(realWidgetId);

  openNewWindowTab("about:blank", { inNewWindow: true, onLoad: function(e) {
    assert.equal(e.target.defaultView.document.getElementById(realWidgetId), null);
    close(e.target.defaultView).then(_ => {
      loader.unload();
      done();
    });
  }}).catch(assert.fail);
};

exports.testWideWidget = function testWideWidget(assert) {
  let { loader } = LoaderWithHookedConsole(module);
  const widgets = loader.require("sdk/widget");
  const { document, CustomizableUI, gCustomizeMode, setTimeout } = getMostRecentBrowserWindow();

  let wideWidget = widgets.Widget({
    id: "my-wide-widget",
    label: "wide-wdgt",
    content: "foo",
    width: 200
  });

  let widget = widgets.Widget({
    id: "my-regular-widget",
    label: "reg-wdgt",
    content: "foo"
  });

  let wideWidgetNode = document.querySelector("toolbaritem[label=wide-wdgt]");
  let widgetNode = document.querySelector("toolbaritem[label=reg-wdgt]");

  assert.equal(wideWidgetNode, null,
    "Wide Widget are not added to UI");

  assert.notEqual(widgetNode, null,
    "regular size widget are in the UI");
};

require("sdk/test").run(exports);
