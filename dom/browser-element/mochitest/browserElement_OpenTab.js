/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1144015 - test middle/ctrl/cmd-click on a link.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  let iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe);

  let x = 2;
  let y = 2;
  // First we force a reflow so that getChildProcessOffset actually returns
  // meaningful data.
  iframe.getBoundingClientRect();
  // We need to make sure the event coordinates are actually inside the iframe,
  // relative to the chome window.
  let tabParent = SpecialPowers.wrap(iframe)
                  .QueryInterface(SpecialPowers.Ci.nsIFrameLoaderOwner)
                  .frameLoader.tabParent;
  if (tabParent) {
    let offsetX = {};
    let offsetY = {};
    tabParent.getChildProcessOffset(offsetX, offsetY);
    x -= offsetX.value;
    y -= offsetY.value;
  }

  let sendCtrlClick = () => {
    let nsIDOMWindowUtils = SpecialPowers.Ci.nsIDOMWindowUtils;
    let mod = nsIDOMWindowUtils.MODIFIER_META |
              nsIDOMWindowUtils.MODIFIER_CONTROL;
    iframe.sendMouseEvent('mousedown', x, y, 0, 1, mod);
    iframe.sendMouseEvent('mouseup', x, y, 0, 1, mod);
  }

  let onCtrlClick = e => {
    is(e.detail.url, 'http://example.com/', 'URL matches');
    iframe.removeEventListener('mozbrowseropentab', onCtrlClick);
    iframe.addEventListener('mozbrowseropentab', onMiddleClick);
    sendMiddleClick();
  }

  let sendMiddleClick = () => {
    iframe.sendMouseEvent('mousedown', x, y, 1, 1, 0);
    iframe.sendMouseEvent('mouseup', x, y, 1, 1, 0);
  }

  let onMiddleClick = e => {
    is(e.detail.url, 'http://example.com/', 'URL matches');
    iframe.removeEventListener('mozbrowseropentab', onMiddleClick);
    SimpleTest.finish();
  }

  iframe.addEventListener('mozbrowserloadend', e => {
    iframe.addEventListener('mozbrowseropentab', onCtrlClick);
    sendCtrlClick();
  });


  iframe.src = 'data:text/html,<body style="margin:0"><a href="http://example.com"><span>click here</span></a></body>';
}

addEventListener('testready', runTest);
