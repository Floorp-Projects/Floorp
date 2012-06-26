"use strict";

SimpleTest.waitForExplicitFinish();

var iframeScript = function() {

  content.fireContextMenu = function(element) {
    var ev = content.document.createEvent('HTMLEvents');
    ev.initEvent('contextmenu', true, false);
    element.dispatchEvent(ev);
  }

  XPCNativeWrapper.unwrap(content).ctxCallbackFired = function(data) {
    sendAsyncMessage('test:callbackfired', {data: data});
  }

  XPCNativeWrapper.unwrap(content).onerror = function(e) {
    sendAsyncMessage('test:errorTriggered', {data: e});
  }

  content.fireContextMenu(content.document.body);
  content.fireContextMenu(content.document.getElementById('menu1-trigger'));
  content.fireContextMenu(content.document.getElementById('inner-link'));
  content.fireContextMenu(content.document.getElementById('menu2-trigger'));
}

var trigger1 = function() {
  content.fireContextMenu(content.document.getElementById('menu1-trigger'));
};

function runTest() {

  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe1 = document.createElement('iframe');
  iframe1.mozbrowser = true;
  document.body.appendChild(iframe1);
  iframe1.src = 'data:text/html,<html>' +
    '<body>' +
    '<menu type="context" id="menu1" label="firstmenu">' +
      '<menuitem label="foo" onclick="window.ctxCallbackFired(\'foo\')"></menuitem>' +
      '<menuitem label="bar" onclick="throw(\'anerror\')"></menuitem>' +
    '</menu>' +
    '<menu type="context" id="menu2" label="secondmenu">' +
      '<menuitem label="outer" onclick="window.ctxCallbackFired(\'err\')"></menuitem>' +
      '<menu>' +
        '<menuitem label="inner 1"></menuitem>' +
        '<menuitem label="inner 2" onclick="window.ctxCallbackFired(\'inner2\')"></menuitem>' +
      '</menu>' +
    '</menu>' +
    '<div id="menu1-trigger" contextmenu="menu1"><a id="inner-link" href="foo.html">Menu 1</a></div>' +
    '<a href="bar.html" contextmenu="menu2"><img id="menu2-trigger" src="example.png" /></a>' +
    '</body></html>';

  var mm;
  var numIframeLoaded = 0;
  var ctxMenuEvents = 0;
  var ctxCallbackEvents = 0;

  var cachedCtxDetail = null;

  // We fire off various contextmenu events to check the data that gets
  // passed to the handler
  function iframeContextmenuHandler(e) {
    var detail = e.detail;
    ctxMenuEvents++;
    if (ctxMenuEvents === 1) {
      ok(detail.contextmenu === null, 'body context clicks have no context menu');
    } else if (ctxMenuEvents === 2) {
      cachedCtxDetail = detail;
      ok(detail.contextmenu.items.length === 2, 'trigger custom contextmenu');
    } else if (ctxMenuEvents === 3) {
      ok(detail.systemTargets.length === 1, 'Includes anchor data');
      ok(detail.contextmenu.items.length === 2, 'Inner clicks trigger correct menu');
    } else if (ctxMenuEvents === 4) {
      var innerMenu = detail.contextmenu.items.filter(function(x) {
        return x.type === 'menu';
      });
      ok(detail.systemTargets.length === 2, 'Includes anchor and img data');
      ok(innerMenu.length > 0, 'Menu contains a nested menu');
      ok(true, 'Got correct number of contextmenu events');
      // Finished testing the data passed to the contextmenu handler,
      // now we start selecting contextmenu items

      // This is previously triggered contextmenu data, since we have
      // fired subsequent contextmenus this should not be mistaken
      // for a current menuitem
      var prevId = cachedCtxDetail.contextmenu.items[0].id;
      cachedCtxDetail.contextMenuItemSelected(prevId);
      // This triggers a current menuitem
      detail.contextMenuItemSelected(innerMenu[0].items[1].id);
      // Once an item it selected, subsequent selections are ignored
      detail.contextMenuItemSelected(innerMenu[0].items[0].id);
    } else if (ctxMenuEvents === 5) {
      ok(detail.contextmenu.label === 'firstmenu', 'Correct menu enabled');
      detail.contextMenuItemSelected(detail.contextmenu.items[0].id);
    } else if (ctxMenuEvents === 6) {
      detail.contextMenuItemSelected(detail.contextmenu.items[1].id);
    } else if (ctxMenuEvents > 6) {
      ok(false, 'Too many events');
    }
  }

  function ctxCallbackRecieved(msg) {
    ctxCallbackEvents++;
    if (ctxCallbackEvents === 1) {
      ok(msg.json.data === 'inner2', 'Callback function got fired correctly');
      mm.loadFrameScript('data:,(' + trigger1.toString() + ')();', false);
    } else if (ctxCallbackEvents === 2) {
      ok(msg.json.data === 'foo', 'Callback function got fired correctly');
      mm.loadFrameScript('data:,(' + trigger1.toString() + ')();', false);
    } else if (ctxCallbackEvents > 2) {
      ok(false, 'Too many callback events');
    }
  }

  function errorTriggered(msg) {
    ok(true, 'An error in the callback triggers window.onerror');
    SimpleTest.finish();
  }

  function iframeLoadedHandler() {
    numIframeLoaded++;
    if (numIframeLoaded === 2) {
      mm = SpecialPowers.getBrowserFrameMessageManager(iframe1);
      mm.addMessageListener('test:callbackfired', ctxCallbackRecieved);
      mm.addMessageListener('test:errorTriggered', errorTriggered);
      mm.loadFrameScript('data:,(' + iframeScript.toString() + ')();', false);
    }
  }

  iframe1.addEventListener('mozbrowsercontextmenu', iframeContextmenuHandler);
  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('load', function() { SimpleTest.executeSoon(runTest); });
