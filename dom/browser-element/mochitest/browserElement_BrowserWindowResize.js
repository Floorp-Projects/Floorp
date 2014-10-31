/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 891763 - Test the mozbrowserresize event
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var srcResizeTo = "data:text/html,       \
    <script type='application/javascript'> \
      window.resizeTo(300, 300);           \
    <\/script>                             \
  ";

  var srcResizeBy = "data:text/html,       \
    <script type='application/javascript'> \
      window.resizeBy(-100, -100);         \
    <\/script>                             \
  ";

  var count = 0;
  function checkSize(iframe) {
    count++;
    is(iframe.clientWidth,  400, "iframe width does not change");
    is(iframe.clientHeight, 400, "iframe height does not change");
    if (count == 2) {
      SimpleTest.finish();
    }
  }

  function testIFrameWithSrc(src) {
    var iframe = document.createElement('iframe');
    iframe.setAttribute('mozbrowser', 'true');
    iframe.style = "border:none; width:400px; height:400px;";
    iframe.src = src;
    iframe.addEventListener("mozbrowserresize", function (e) {
      is(e.detail.width,  300, "Received correct resize event width");
      is(e.detail.height, 300, "Received correct resize event height");
      SimpleTest.executeSoon(checkSize.bind(undefined, iframe));
    });
    document.body.appendChild(iframe);
  }

  testIFrameWithSrc(srcResizeTo);
  testIFrameWithSrc(srcResizeBy);
}

addEventListener('testready', runTest);
