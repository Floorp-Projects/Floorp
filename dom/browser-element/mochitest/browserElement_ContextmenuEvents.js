'use strict';

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

let audioUrl = 'http://mochi.test:8888/tests/dom/browser-element/mochitest/audio.ogg';
let videoUrl = 'http://mochi.test:8888/tests/dom/browser-element/mochitest/short-video.ogv';

function runTests() {
  createIframe(function onIframeLoaded() {
    checkEmptyContextMenu();
  });
}

function checkEmptyContextMenu() {
  sendContextMenuTo('body', function onContextMenu(detail) {
    is(detail.contextmenu, null, 'Body context clicks have no context menu');

    checkInnerContextMenu();
  });
}

function checkInnerContextMenu() {
  sendContextMenuTo('#inner-link', function onContextMenu(detail) {
    is(detail.systemTargets.length, 1, 'Includes anchor data');
    is(detail.contextmenu.items.length, 2, 'Inner clicks trigger correct menu');
    var target = detail.systemTargets[0];
    is(target.nodeName, 'A', 'Reports correct nodeName');
    is(target.data.uri, 'foo.html', 'Reports correct uri');
    is(target.data.text, 'Menu 1', 'Reports correct link text');

    checkCustomContextMenu();
  });
}

function checkCustomContextMenu() {
  sendContextMenuTo('#menu1-trigger', function onContextMenu(detail) {
    is(detail.contextmenu.items.length, 2, 'trigger custom contextmenu');

    checkNestedContextMenu();
  });
}

function checkNestedContextMenu() {
  sendContextMenuTo('#menu2-trigger', function onContextMenu(detail) {
    var innerMenu = detail.contextmenu.items.filter(function(x) {
      return x.type === 'menu';
    });
    is(detail.systemTargets.length, 2, 'Includes anchor and img data');
    ok(innerMenu.length > 0, 'Menu contains a nested menu');

    checkPreviousContextMenuHandler();
  });
}

 // Finished testing the data passed to the contextmenu handler,
 // now we start selecting contextmenu items
function checkPreviousContextMenuHandler() {
  // This is previously triggered contextmenu data, since we have
  // fired subsequent contextmenus this should not be mistaken
  // for a current menuitem
  var detail = previousContextMenuDetail;
  var previousId = detail.contextmenu.items[0].id;
  checkContextMenuCallbackForId(detail, previousId, function onCallbackFired(label) {
    is(label, null, 'Callback label should be empty since this handler is old');

    checkCurrentContextMenuHandler();
  });
}

function checkCurrentContextMenuHandler() {
  // This triggers a current menuitem
  var detail = currentContextMenuDetail;

  var innerMenu = detail.contextmenu.items.filter(function(x) {
    return x.type === 'menu';
  });

  var currentId = innerMenu[0].items[1].id;
  checkContextMenuCallbackForId(detail, currentId, function onCallbackFired(label) {
    is(label, 'inner 2', 'Callback label should be set correctly');

    checkAgainCurrentContextMenuHandler();
  });
}

function checkAgainCurrentContextMenuHandler() {
  // Once an item it selected, subsequent selections are ignored
  var detail = currentContextMenuDetail;

  var innerMenu = detail.contextmenu.items.filter(function(x) {
    return x.type === 'menu';
  });

  var currentId = innerMenu[0].items[1].id;
  checkContextMenuCallbackForId(detail, currentId, function onCallbackFired(label) {
    is(label, null, 'Callback label should be empty since this handler has already been used');

    checkCallbackWithPreventDefault();
  });
};

// Finished testing callbacks if the embedder calls preventDefault() on the
// mozbrowsercontextmenu event, now we start checking for some cases where the embedder
// does not want to call preventDefault() for some reasons.
function checkCallbackWithPreventDefault() {
  sendContextMenuTo('#menu1-trigger', function onContextMenu(detail) {
    var id = detail.contextmenu.items[0].id;
    checkContextMenuCallbackForId(detail, id, function onCallbackFired(label) {
      is(label, 'foo', 'Callback label should be set correctly');

      checkCallbackWithoutPreventDefault();
    });
  });
}

function checkCallbackWithoutPreventDefault() {
  sendContextMenuTo('#menu1-trigger', function onContextMenu(detail) {
    var id = detail.contextmenu.items[0].id;
    checkContextMenuCallbackForId(detail, id, function onCallbackFired(label) {
      is(label, null, 'Callback label should be null');

      checkImageContextMenu();
    });
  }, /* ignorePreventDefault */ true);
}

function checkImageContextMenu() {
  sendContextMenuTo('#menu3-trigger', function onContextMenu(detail) {
    var target = detail.systemTargets[0];
    is(target.nodeName, 'IMG', 'Reports correct nodeName');
    is(target.data.uri, 'example.png', 'Reports correct uri');

    checkVideoContextMenu();
  }, /* ignorePreventDefault */ true);
}

function checkVideoContextMenu() {
  sendContextMenuTo('#menu4-trigger', function onContextMenu(detail) {
    var target = detail.systemTargets[0];
    is(target.nodeName, 'VIDEO', 'Reports correct nodeName');
    is(target.data.uri, videoUrl, 'Reports uri correctly in data');
    is(target.data.hasVideo, true, 'Video data in video tag does "hasVideo"');

    checkAudioContextMenu();
  }, /* ignorePreventDefault */ true);
}

function checkAudioContextMenu() {
  sendContextMenuTo('#menu6-trigger', function onContextMenu(detail) {
    var target = detail.systemTargets[0];
    is(target.nodeName, 'AUDIO', 'Reports correct nodeName');
    is(target.data.uri, audioUrl, 'Reports uri correctly in data');

    checkAudioinVideoContextMenu();
  }, /* ignorePreventDefault */ true);
}

function checkAudioinVideoContextMenu() {
  sendSrcTo('#menu5-trigger', audioUrl, function onSrcSet() {
    sendContextMenuTo('#menu5-trigger', function onContextMenu(detail) {
      var target = detail.systemTargets[0];
      is(target.nodeName, 'VIDEO', 'Reports correct nodeName');
      is(target.data.uri, audioUrl, 'Reports uri correctly in data');
      is(target.data.hasVideo, false, 'Audio data in video tag reports no "hasVideo"');

      checkFormNoMethod();
    }, /* ignorePreventDefault */ true);
  });
}

function checkFormNoMethod() {
  sendContextMenuTo('#menu7-trigger', function onContextMenu(detail) {
    var target = detail.systemTargets[0];
    is(target.nodeName, 'INPUT', 'Reports correct nodeName');
    is(target.data.method, 'get', 'Reports correct method');
    is(target.data.action, 'no_method', 'Reports correct action url');
    is(target.data.name, 'input1', 'Reports correct input name');

    checkFormGetMethod();
  }, /* ignorePreventDefault */ true);
}

function checkFormGetMethod() {
  sendContextMenuTo('#menu8-trigger', function onContextMenu(detail) {
    var target = detail.systemTargets[0];
    is(target.nodeName, 'INPUT', 'Reports correct nodeName');
    is(target.data.method, 'get', 'Reports correct method');
    is(target.data.action, 'http://example.com/get_method', 'Reports correct action url');
    is(target.data.name, 'input2', 'Reports correct input name');

    checkFormPostMethod();
  }, /* ignorePreventDefault */ true);
}

function checkFormPostMethod() {
  sendContextMenuTo('#menu9-trigger', function onContextMenu(detail) {
    var target = detail.systemTargets[0];
    is(target.nodeName, 'INPUT', 'Reports correct nodeName');
    is(target.data.method, 'post', 'Reports correct method');
    is(target.data.action, 'post_method', 'Reports correct action url');
    is(target.data.name, 'input3', 'Reports correct input name');

    SimpleTest.finish();
  }, /* ignorePreventDefault */ true);
}

/* Helpers */
var mm = null;
var previousContextMenuDetail = null;
var currentContextMenuDetail = null;

function sendSrcTo(selector, src, callback) {
  mm.sendAsyncMessage('setsrc', { 'selector': selector, 'src': src });
  mm.addMessageListener('test:srcset', function onSrcSet(msg) {
    mm.removeMessageListener('test:srcset', onSrcSet);
    callback();
  });
}

function sendContextMenuTo(selector, callback, ignorePreventDefault) {
  iframe.addEventListener('mozbrowsercontextmenu', function oncontextmenu(e) {
    iframe.removeEventListener(e.type, oncontextmenu);

    // The embedder should call preventDefault() on the event if it will handle
    // it. Not calling preventDefault() means it won't handle the event and 
    // should not be able to deal with context menu callbacks.
    if (ignorePreventDefault !== true) {
      e.preventDefault();
    }

    // Keep a reference to previous/current contextmenu event details.
    previousContextMenuDetail = currentContextMenuDetail;
    currentContextMenuDetail = e.detail;

    setTimeout(function() { callback(e.detail); });
  });

  mm.sendAsyncMessage('contextmenu', { 'selector': selector });
}

function checkContextMenuCallbackForId(detail, id, callback) {
  mm.addMessageListener('test:callbackfired', function onCallbackFired(msg) {
    mm.removeMessageListener('test:callbackfired', onCallbackFired);

    msg = SpecialPowers.wrap(msg);
    setTimeout(function() { callback(msg.data.label); });
  });

  detail.contextMenuItemSelected(id);
}


var iframe = null;
function createIframe(callback) {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  iframe.src = 'data:text/html,<html>' +
    '<body>' +
    '<menu type="context" id="menu1" label="firstmenu">' +
      '<menuitem label="foo" onclick="window.onContextMenuCallbackFired(event)"></menuitem>' +
      '<menuitem label="bar" onclick="window.onContextMenuCallbackFired(event)"></menuitem>' +
    '</menu>' +
    '<menu type="context" id="menu2" label="secondmenu">' +
      '<menuitem label="outer" onclick="window.onContextMenuCallbackFired(event)"></menuitem>' +
      '<menu>' +
        '<menuitem label="inner 1"></menuitem>' +
        '<menuitem label="inner 2" onclick="window.onContextMenuCallbackFired(event)"></menuitem>' +
      '</menu>' +
    '</menu>' +
    '<div id="menu1-trigger" contextmenu="menu1"><a id="inner-link" href="foo.html">Menu 1</a></div>' +
    '<a href="bar.html" contextmenu="menu2"><img id="menu2-trigger" src="example.png" /></a>' +
    '<img id="menu3-trigger" src="example.png" />' +
    '<video id="menu4-trigger" src="' + videoUrl + '"></video>' +
    '<video id="menu5-trigger" preload="metadata"></video>' +
    '<audio id="menu6-trigger" src="' + audioUrl + '"></audio>' +
    '<form action="no_method"><input id="menu7-trigger" name="input1"></input></form>' +
    '<form action="http://example.com/get_method" method="get"><input id="menu8-trigger" name="input2"></input></form>' +
    '<form action="post_method" method="post"><input id="menu9-trigger" name="input3"></input></form>' +
    '</body></html>';
  document.body.appendChild(iframe);

  // The following code will be included in the child
  // =========================================================================
  function iframeScript() {
    addMessageListener('contextmenu', function onContextMenu(msg) {
      var document = content.document;
      var evt = document.createEvent('HTMLEvents');
      evt.initEvent('contextmenu', true, true);
      document.querySelector(msg.data.selector).dispatchEvent(evt);
    });

    addMessageListener('setsrc', function onContextMenu(msg) {
      var wrappedTarget = content.document.querySelector(msg.data.selector);
      var target = XPCNativeWrapper.unwrap(wrappedTarget);
      target.addEventListener('loadedmetadata', function() {
        sendAsyncMessage('test:srcset');
      });
      target.src = msg.data.src;
    });

    addMessageListener('browser-element-api:call', function onCallback(msg) {
      if (msg.data.msg_name != 'fire-ctx-callback')
        return;

      /* Use setTimeout in order to react *after* the platform */
      content.setTimeout(function() {
        sendAsyncMessage('test:callbackfired', { label: label });
        label = null;
      });
    });

    var label = null;
    XPCNativeWrapper.unwrap(content).onContextMenuCallbackFired = function(e) {
      label = e.target.getAttribute('label');
    };
  }
  // =========================================================================

  iframe.addEventListener('mozbrowserloadend', function onload(e) {
    iframe.removeEventListener(e.type, onload);
    mm = SpecialPowers.getBrowserFrameMessageManager(iframe);
    mm.loadFrameScript('data:,(' + iframeScript.toString() + ')();', false);

    // Now we're ready, let's start testing.
    callback();
  });
}

addEventListener('testready', runTests);
