/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const widgets = require("sdk/widget");
const { Cc, Ci } = require("chrome");
const { Loader } = require('sdk/test/loader');
const url = require("sdk/url");
const timer = require("sdk/timers");
const self = require("sdk/self");
const windowUtils = require("sdk/deprecated/window-utils");
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { close } = require("sdk/window/helpers");

let jetpackID = "testID";
try {
  jetpackID = require("sdk/self").id;
} catch(e) {}

const australis = !!require("sdk/window/utils").getMostRecentBrowserWindow().CustomizableUI;

exports.testConstructor = function(assert, done) {
  let browserWindow = windowUtils.activeBrowserWindow;
  let doc = browserWindow.document;
  let AddonsMgrListener;
  if (australis) {
    AddonsMgrListener = {
      onInstalling: () => {},
      onInstalled: () => {},
      onUninstalling: () => {},
      onUninstalled: () => {}
    };
  }
  else {
    AddonsMgrListener = browserWindow.AddonsMgrListener;
  }

  function container() australis ? doc.getElementById("nav-bar") : doc.getElementById("addon-bar");
  function getWidgets() container() ? container().querySelectorAll('[id^="widget\:"]') : [];
  function widgetCount() getWidgets().length;
  let widgetStartCount = widgetCount();
  function widgetNode(index) getWidgets()[index];

  // Test basic construct/destroy
  AddonsMgrListener.onInstalling();
  let w = widgets.Widget({ id: "fooID", label: "foo", content: "bar" });
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
  let loader = Loader(module);
  let widgetsFromLoader = loader.require("sdk/widget");
  let widgetStartCount = widgetCount();
  let w = widgetsFromLoader.Widget({ id: "fooID", label: "foo", content: "bar" });
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
    function() widgets.Widget({id: "fooID", label: "foo"}),
    /^No content or contentURL property found\. Widgets must have one or the other\.$/,
    "throws on no content");

  // Test empty content, no image
  assert.throws(
    function() widgets.Widget({id:"fooID", label: "foo", content: ""}),
    /^No content or contentURL property found\. Widgets must have one or the other\.$/,
    "throws on empty content");

  // Test empty image, no content
  assert.throws(
    function() widgets.Widget({id:"fooID", label: "foo", image: ""}),
    /^No content or contentURL property found\. Widgets must have one or the other\.$/,
    "throws on empty content");

  // Test empty content, empty image
  assert.throws(
    function() widgets.Widget({id:"fooID", label: "foo", content: "", image: ""}),
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
  let w1 = widgets.Widget({id: "first", label:"first", content: "bar"});
  let w2 = widgets.Widget({id: "second", label:"second", content: "bar"});
  let w3 = widgets.Widget({id: "third", label:"third", content: "bar"});
  // Remove the middle widget
  assert.equal(widgetNode(1).getAttribute("label"), "second", "second widget is the second widget inserted");
  w2.destroy();
  assert.equal(widgetNode(1).getAttribute("label"), "third", "second widget is removed, so second widget is now the third one");
  w2 = widgets.Widget({id: "second", label:"second", content: "bar"});
  assert.equal(widgetNode(1).getAttribute("label"), "second", "second widget is created again, at the same location");
  // Cleanup this testcase
  AddonsMgrListener.onUninstalling();
  w1.destroy();
  w2.destroy();
  w3.destroy();
  AddonsMgrListener.onUninstalled();

  // Test concurrent widget module instances on addon-bar hiding
  if (!australis) {
    let loader = Loader(module);
    let anotherWidgetsInstance = loader.require("sdk/widget");
    assert.ok(container().collapsed, "UI is hidden when no widgets");
    AddonsMgrListener.onInstalling();
    let w1 = widgets.Widget({id: "foo", label: "foo", content: "bar"});
    // Ideally we would let AddonsMgrListener display the addon bar
    // But, for now, addon bar is immediatly displayed by sdk code
    // https://bugzilla.mozilla.org/show_bug.cgi?id=627484
    assert.ok(!container().collapsed, "UI is already visible when we just added the widget");
    AddonsMgrListener.onInstalled();
    assert.ok(!container().collapsed, "UI become visible when we notify AddonsMgrListener about end of addon installation");
    let w2 = anotherWidgetsInstance.Widget({id: "bar", label: "bar", content: "foo"});
    assert.ok(!container().collapsed, "UI still visible when we add a second widget");
    AddonsMgrListener.onUninstalling();
    w1.destroy();
    AddonsMgrListener.onUninstalled();
    assert.ok(!container().collapsed, "UI still visible when we remove one of two widgets");
    AddonsMgrListener.onUninstalling();
    w2.destroy();
    assert.ok(!container().collapsed, "UI is still visible when we have removed all widget but still not called onUninstalled");
    AddonsMgrListener.onUninstalled();
    assert.ok(container().collapsed, "UI is hidden when we have removed all widget and called onUninstalled");
  }
  // Helper for testing a single widget.
  // Confirms proper addition and content setup.
  function testSingleWidget(widgetOptions) {
    // We have to display which test is being run, because here we do not
    // use the regular test framework but rather a custom one that iterates
    // the `tests` array.
    console.info("executing: " + widgetOptions.id);

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
    if (!tests.length)
      done();
    else
      timer.setTimeout(tests.shift(), 0);
  }
  function doneTest() nextTest();

  // text widget
  tests.push(function testTextWidget() testSingleWidget({
    id: "text",
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
    contentURL: require("sdk/self").data.url("test.html"),
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
    contentURL: require("sdk/self").data.url("test.html"),
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
    id: "click",
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
    id: "mouseover",
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
    id: "mouseout",
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
    id: "click",
    label: "click test widget - image",
    contentURL: require("sdk/self").data.url("moz_favicon.ico"),
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
    id: "mouseover",
    label: "mouseover test widget - image",
    contentURL: require("sdk/self").data.url("moz_favicon.ico"),
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
    id: "mouseout",
    label: "mouseout test widget - image",
    contentURL: require("sdk/self").data.url("moz_favicon.ico"),
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
    id: "content",
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
    id: "content",
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
    id: "text",
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
    id: "tooltip",
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
    id: "allow",
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
    id: "allow",
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
    id: "allow",
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
    const tabBrowser = require("sdk/deprecated/tab-browser");

    tabBrowser.addTab("about:blank", { inNewWindow: true, onLoad: function(e) {
      let browserWindow = e.target.defaultView;
      let doc = browserWindow.document;
      function container() australis ? doc.getElementById("nav-bar") : doc.getElementById("addon-bar");
      function widgetCount2() container() ? container().querySelectorAll('[id^="widget\:"]').length : 0;
      let widgetStartCount2 = widgetCount2();

      let w1Opts = {id:"first", label: "first widget", content: "first content"};
      let w1 = testSingleWidget(w1Opts);
      assert.equal(widgetCount2(), widgetStartCount2 + 1, "2nd window has correct number of child elements after first widget");

      let w2Opts = {id:"second", label: "second widget", content: "second content"};
      let w2 = testSingleWidget(w2Opts);
      assert.equal(widgetCount2(), widgetStartCount2 + 2, "2nd window has correct number of child elements after second widget");

      w1.destroy();
      assert.equal(widgetCount2(), widgetStartCount2 + 1, "2nd window has correct number of child elements after first destroy");
      w2.destroy();
      assert.equal(widgetCount2(), widgetStartCount2, "2nd window has correct number of child elements after second destroy");

      close(browserWindow).then(doneTest);
    }});
  });

  // test window closing
  tests.push(function testWindowClosing() {
    // 1/ Create a new widget
    let w1Opts = {
      id:"first",
      label: "first widget",
      content: "first content",
      contentScript: "self.port.on('event', function () self.port.emit('event'))"
    };
    let widget = testSingleWidget(w1Opts);
    let windows = require("sdk/windows").browserWindows;

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

  if (!australis) {
    tests.push(function testAddonBarHide() {
      const tabBrowser = require("sdk/deprecated/tab-browser");

      // Hide the addon-bar
      browserWindow.setToolbarVisibility(container(), false);
      assert.ok(container().collapsed,
                "1st window starts with an hidden addon-bar");

      // Then open a browser window and verify that the addon-bar remains hidden
      tabBrowser.addTab("about:blank", { inNewWindow: true, onLoad: function(e) {
        let browserWindow2 = e.target.defaultView;
        let doc2 = browserWindow2.document;
        function container2() doc2.getElementById("addon-bar");
        function widgetCount2() container2() ? container2().childNodes.length : 0;
        let widgetStartCount2 = widgetCount2();
        assert.ok(container2().collapsed,
                  "2nd window starts with an hidden addon-bar");

        let w1Opts = {id:"first", label: "first widget", content: "first content"};
        let w1 = testSingleWidget(w1Opts);
        assert.equal(widgetCount2(), widgetStartCount2 + 1,
                     "2nd window has correct number of child elements after" +
                     "widget creation");
        w1.destroy();
        assert.equal(widgetCount2(), widgetStartCount2,
                     "2nd window has correct number of child elements after" +
                     "widget destroy");

        assert.ok(container().collapsed, "1st window has an hidden addon-bar");
        assert.ok(container2().collapsed, "2nd window has an hidden addon-bar");

        // Reset addon-bar visibility before exiting this test
        browserWindow.setToolbarVisibility(container(), true);

        close(browserWindow2).then(doneTest);
      }});
    });
  }

  // test widget.width
  tests.push(function testWidgetWidth() testSingleWidget({
    id: "text",
    label: "test widget.width",
    content: "test width",
    width: 200,
    contentScript: "self.postMessage(1)",
    contentScriptWhen: "ready",
    onMessage: function(message) {
      assert.equal(this.width, 200, 'width is 200');

      let node = widgetNode(0);
      assert.equal(this.width, node.style.minWidth.replace("px", ""));
      assert.equal(this.width, node.firstElementChild.style.width.replace("px", ""));
      this.width = 300;
      assert.equal(this.width, node.style.minWidth.replace("px", ""));
      assert.equal(this.width, node.firstElementChild.style.width.replace("px", ""));

      this.destroy();
      doneTest();
    }
  }));

  // test click handler not respond to right-click
  let clickCount = 0;
  tests.push(function testNoRightClick() testSingleWidget({
    id: "click-content",
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
  const widgets = require("sdk/widget");

  let widget1 = widgets.Widget({
    id: "panel1",
    label: "panel widget 1",
    content: "<div id='me'>foo</div>",
    contentScript: "var evt = new MouseEvent('click', {button: 0});" +
                   "document.body.dispatchEvent(evt);",
    contentScriptWhen: "end",
    panel: require("sdk/panel").Panel({
      contentURL: "data:text/html;charset=utf-8,<body>Look ma, a panel!</body>",
      onShow: function() {
        let { document } = getMostRecentBrowserWindow();
        let widgetEle = document.getElementById("widget:" + jetpackID + "-" + widget1.id);
        let panelEle = document.getElementById('mainPopupSet').lastChild;
        // See bug https://bugzilla.mozilla.org/show_bug.cgi?id=859592
        assert.equal(panelEle.getAttribute("type"), "arrow", 'the panel is a arrow type');
        assert.strictEqual(panelEle.anchorNode, widgetEle, 'the panel is properly anchored to the widget');

        widget1.destroy();
        assert.pass("panel displayed on click");
        done();
      }
    })
  });
};

exports.testWidgetWithInvalidPanel = function(assert) {
  const widgets = require("sdk/widget");
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
};

exports.testPanelWidget3 = function testPanelWidget3(assert, done) {
  const widgets = require("sdk/widget");
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
    panel: require("sdk/panel").Panel({
      contentURL: "data:text/html;charset=utf-8,<body>Look ma, a panel!</body>",
      onShow: function() {
        assert.ok(
          onClickCalled,
          "onClick called on click for widget with both panel and onClick");
        widget3.destroy();
        done();
      }
    })
  });
};

exports.testWidgetMessaging = function testWidgetMessaging(assert, done) {
  let origMessage = "foo";
  const widgets = require("sdk/widget");
  let widget = widgets.Widget({
    id: "foo",
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
        done();
      }
    }
  });
};

exports.testWidgetViews = function testWidgetViews(assert, done) {
  const widgets = require("sdk/widget");
  let widget = widgets.Widget({
    id: "foo",
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
        done();
      });
    }
  });
};

exports.testWidgetViewsUIEvents = function testWidgetViewsUIEvents(assert, done) {
  const widgets = require("sdk/widget");
  let view = null;
  let widget = widgets.Widget({
    id: "foo",
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
      let view2 = widget.getView(require("sdk/windows").browserWindows.activeWindow);
      assert.equal(view, view2,
                         "widget.getView return the same WidgetView");
      widget.destroy();
      done();
    }
  });
};

exports.testWidgetViewsCustomEvents = function testWidgetViewsCustomEvents(assert, done) {
  const widgets = require("sdk/widget");
  let widget = widgets.Widget({
    id: "foo",
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
    done();
  });
};

exports.testWidgetViewsTooltip = function testWidgetViewsTooltip(assert, done) {
  const widgets = require("sdk/widget");

  let widget = new widgets.Widget({
    id: "foo",
    label: "foo",
    content: "foo"
  });
  let view = widget.getView(require("sdk/windows").browserWindows.activeWindow);
  widget.tooltip = null;
  assert.equal(view.tooltip, "foo",
               "view tooltip defaults to base widget label");
  assert.equal(widget.tooltip, "foo",
               "tooltip defaults to base widget label");
  widget.destroy();
  done();
};

exports.testWidgetMove = function testWidgetMove(assert, done) {
  let windowUtils = require("sdk/deprecated/window-utils");
  let widgets = require("sdk/widget");

  let browserWindow = windowUtils.activeBrowserWindow;
  let doc = browserWindow.document;

  let label = "unique-widget-label";
  let origMessage = "message after node move";
  let gotFirstReady = false;

  let widget = widgets.Widget({
    id: "foo",
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
  function getWidgetContent(widget) {
    let windowUtils = require("sdk/deprecated/window-utils");
    let browserWindow = windowUtils.activeBrowserWindow;
    let doc = browserWindow.document;
    let widgetNode = doc.querySelector('toolbaritem[label="' + widget.label + '"]');
    assert.ok(widgetNode, 'found widget node in the front-end');
    return widgetNode.firstChild.contentDocument.body.innerHTML;
  }

  let widgets = require("sdk/widget");
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
        done();
      }
    }
  });
};

exports.testContentScriptOptionsOption = function(assert, done) {
  let widget = require("sdk/widget").Widget({
      id: "fooz",
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
        done();
      }
    });
};

exports.testOnAttachWithoutContentScript = function(assert, done) {
  let widget = require("sdk/widget").Widget({
      id: "onAttachNoCS",
      label: "onAttachNoCS",
      content: "onAttachNoCS",
      onAttach: function (view) {
        assert.pass("received attach event");
        widget.destroy();
        done();
      }
    });
};

exports.testPostMessageOnAttach = function(assert, done) {
  let widget = require("sdk/widget").Widget({
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
        done();
      }
    });
};

exports.testPostMessageOnLocationChange = function(assert, done) {
  let attachEventCount = 0;
  let messagesCount = 0;
  let widget = require("sdk/widget").Widget({
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
          done();
        }
      }
    });
};

exports.testSVGWidget = function(assert, done) {
  // use of capital SVG here is intended, that was failing..
  let SVG_URL = self.data.url("mofo_logo.SVG");

  let widget = require("sdk/widget").Widget({
    id: "mozilla-svg-logo",
    label: "moz foundation logo",
    contentURL: SVG_URL,
    contentScript: "self.postMessage({count: window.document.images.length, src: window.document.images[0].src});",
    onMessage: function(data) {
      widget.destroy();
      assert.equal(data.count, 1, 'only one image');
      assert.equal(data.src, SVG_URL, 'only one image');
      done();
    }
  });
};

if (!australis) {
  exports.testNavigationBarWidgets = function testNavigationBarWidgets(assert, done) {
    let w1 = widgets.Widget({id: "1st", label: "1st widget", content: "1"});
    let w2 = widgets.Widget({id: "2nd", label: "2nd widget", content: "2"});
    let w3 = widgets.Widget({id: "3rd", label: "3rd widget", content: "3"});

    // First wait for all 3 widgets to be added to the current browser window
    let firstAttachCount = 0;
    function onAttachFirstWindow(widget) {
      if (++firstAttachCount<3)
        return;
      onWidgetsReady();
    }
    w1.once("attach", onAttachFirstWindow);
    w2.once("attach", onAttachFirstWindow);
    w3.once("attach", onAttachFirstWindow);

    function getWidgetNode(toolbar, position) {
      return toolbar.getElementsByTagName("toolbaritem")[position];
    }
    function openBrowserWindow() {
      let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
               getService(Ci.nsIWindowWatcher);
      let urlString = Cc["@mozilla.org/supports-string;1"].
                      createInstance(Ci.nsISupportsString);
      urlString.data = "about:blank";
      return ww.openWindow(null, "chrome://browser/content/browser.xul",
                                 "_blank", "chrome,all,dialog=no", urlString);
    }

    // Then move them before openeing a new browser window
    function onWidgetsReady() {
      // Hack to move 2nd and 3rd widgets manually to the navigation bar right after
      // the search box.
      let browserWindow = windowUtils.activeBrowserWindow;
      let doc = browserWindow.document;
      let addonBar = doc.getElementById("addon-bar");
      let w2ToolbarItem = getWidgetNode(addonBar, 1);
      let w3ToolbarItem = getWidgetNode(addonBar, 2);
      let navBar = doc.getElementById("nav-bar");
      let searchBox = doc.getElementById("search-container");
      // Insert 3rd at the right of search box by adding it before its right sibling
      navBar.insertItem(w3ToolbarItem.id, searchBox.nextSibling, null, false);
      // Then insert 2nd before 3rd
      navBar.insertItem(w2ToolbarItem.id, w3ToolbarItem, null, false);
      // Widget and Firefox codes rely on this `currentset` attribute,
      // so ensure it is correctly saved
      navBar.setAttribute("currentset", navBar.currentSet);
      doc.persist(navBar.id, "currentset");
      // Update addonbar too as we removed widget from there.
      // Otherwise, widgets may still be added to this toolbar.
      addonBar.setAttribute("currentset", addonBar.currentSet);
      doc.persist(addonBar.id, "currentset");

      // Wait for all widget to be attached to this new window before checking
      // their position
      let attachCount = 0;
      let browserWindow2;
      function onAttach(widget) {
        if (++attachCount < 3)
          return;
        let doc = browserWindow2.document;
        let addonBar = doc.getElementById("addon-bar");
        let searchBox = doc.getElementById("search-container");

        // Ensure that 1st is in addon bar
        assert.equal(getWidgetNode(addonBar, 0).getAttribute("label"), w1.label);
        // And that 2nd and 3rd keep their original positions in navigation bar,
        // i.e. right after search box
        assert.equal(searchBox.nextSibling.getAttribute("label"), w2.label);
        assert.equal(searchBox.nextSibling.nextSibling.getAttribute("label"), w3.label);

        w1.destroy();
        w2.destroy();
        w3.destroy();

        close(browserWindow2).then(done);
      }
      w1.on("attach", onAttach);
      w2.on("attach", onAttach);
      w3.on("attach", onAttach);

      browserWindow2 = openBrowserWindow(browserWindow);
    }
  };
}

require("sdk/test").run(exports);
