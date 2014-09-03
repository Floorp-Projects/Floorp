/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 757859 - Test the getContentDimensions functionality of mozbrowser

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var resizeContent = function() {
  var innerBox = content.document.getElementById('abox');
  innerBox.style.width = '800px';
  innerBox.style.height = '800px';
}

function runTest() {

  var iframe1 = document.createElement('iframe');
  SpecialPowers.wrap(iframe1).mozbrowser = true;

  var iframeWidth = 400;
  var iframeHeight = 400;
  var numIframeLoaded = 0;
  var numResizeEvents = 0;
  var mm;

  iframe1.src = 'data:text/html,<html><body><div id=\'abox\' ' +
    'style=\'background:blue;width:200px;height:200px\'>test</div></body></html>';
  iframe1.style.width = iframeWidth + 'px';
  iframe1.style.height = iframeHeight + 'px';
  document.body.appendChild(iframe1);

  function iframeScrollAreaChanged(e) {
    numResizeEvents++;
    if (numResizeEvents === 1) {
      ok(true, 'Resize event when changing content size');
      ok(e.detail.width > iframeWidth, 'Iframes content is larger than iframe');
      ok(e.detail.height > iframeHeight, 'Iframes content is larger than iframe');
      iframe1.src = 'data:text/html,<html><body><div id=\'abox\' ' +
        'style=\'background:blue;width:200px;height:200px\'>test</div></body></html>';
    } else if (numResizeEvents === 2) {
      ok(true, 'Resize event when changing src');
      iframe1.removeEventListener('mozbrowserresize', iframeScrollAreaChanged);
      SimpleTest.finish();
    }
  }

  function iframeLoadedHandler() {
    iframe1.removeEventListener('mozbrowserloadend', iframeLoadedHandler);
    mm = SpecialPowers.getBrowserFrameMessageManager(iframe1);
    iframe1.getContentDimensions().onsuccess = function(e) {
      ok(typeof e.target.result.width === 'number', 'Received width');
      ok(typeof e.target.result.height === 'number', 'Received height');
      ok(e.target.result.height <= iframeHeight, 'Iframes content is smaller than iframe');
      ok(e.target.result.width <= iframeWidth, 'Iframes content is smaller than iframe');
      iframe1.addEventListener('mozbrowserscrollareachanged', iframeScrollAreaChanged);
      mm.loadFrameScript('data:,(' + resizeContent.toString() + ')();', false);
    }
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('load', function() {
  SimpleTest.executeSoon(runTest);
});
