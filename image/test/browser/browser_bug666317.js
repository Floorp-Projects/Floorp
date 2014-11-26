"use strict";

waitForExplicitFinish();

let pageSource =
  '<html><meta charset=UTF-8><body>' +
    '<img id="testImg" src="' + TESTROOT + 'big.png">' +
  '</body></html>';

let oldDiscardingPref, oldTab, newTab;
let prefBranch = Cc["@mozilla.org/preferences-service;1"]
                   .getService(Ci.nsIPrefService)
                   .getBranch('image.mem.');

function imgDiscardingFrameScript() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  function ImageDiscardObserver(result) {
    this.discard = function onDiscard(request) {
      result.wasDiscarded = true;
    }
  }

  function currentRequest() {
    let img = content.document.getElementById('testImg');
    img.QueryInterface(Ci.nsIImageLoadingContent);
    return img.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
  }

  function attachDiscardObserver(result) {
    // Create the discard observer.
    let observer = new ImageDiscardObserver(result);
    let scriptedObserver = Cc["@mozilla.org/image/tools;1"]
                             .getService(Ci.imgITools)
                             .createScriptedObserver(observer);

    // Clone the current imgIRequest with our new observer.
    let request = currentRequest();
    return [ request.clone(scriptedObserver), scriptedObserver ];
  }

  // Attach a discard listener and create a place to hold the result.
  var result = { wasDiscarded: false };
  var scriptedObserver;
  var clonedRequest;

  addMessageListener("test666317:testPart1", function(message) {
    // Ensure that the image is decoded by drawing it to a canvas.
    let doc = content.document;
    let img = doc.getElementById('testImg');
    let canvas = doc.createElement('canvas');
    let ctx = canvas.getContext('2d');
    ctx.drawImage(img, 0, 0);

    // Verify that we decoded the image.
    // Note: We grab a reference to the scripted observer because the image
    // holds a weak ref to it. If we don't hold a strong reference, we might
    // end up using a deleted pointer.
    [ clonedRequest, scriptedObserver ] = attachDiscardObserver(result)
    let decoded = clonedRequest.imageStatus & Ci.imgIRequest.STATUS_FRAME_COMPLETE ? true : false;

    message.target.sendAsyncMessage("test666317:testPart1:Answer",
                                    { decoded });
  });

  addMessageListener("test666317:wasImgDiscarded", function(message) {
    let discarded = result.wasDiscarded;

    // NOTE: We know that this is the last test.
    clonedRequest.cancelAndForgetObserver(0);
    scriptedObserver = null;
    message.target.sendAsyncMessage("test666317:wasImgDiscarded:Answer",
                                    { discarded });
  });
}

function test() {
  // Enable the discarding pref.
  oldDiscardingPref = prefBranch.getBoolPref('discardable');
  prefBranch.setBoolPref('discardable', true);

  // Create and focus a new tab.
  oldTab = gBrowser.selectedTab;
  newTab = gBrowser.addTab('data:text/html,' + pageSource);
  gBrowser.selectedTab = newTab;

  // Run step2 after the tab loads.
  gBrowser.getBrowserForTab(newTab)
          .addEventListener("pageshow", step2 );
}

function step2() {
  let mm = gBrowser.getBrowserForTab(newTab).QueryInterface(Ci.nsIFrameLoaderOwner)
           .frameLoader.messageManager;
  mm.loadFrameScript("data:,(" + escape(imgDiscardingFrameScript.toString()) + ")();", false);

  // Check that the image is decoded.
  mm.addMessageListener("test666317:testPart1:Answer", function(message) {
    let decoded = message.data.decoded;
    ok(decoded, 'Image should initially be decoded.');

    // Focus the old tab, then fire a memory-pressure notification.  This should
    // cause the decoded image in the new tab to be discarded.
    gBrowser.selectedTab = oldTab;
    waitForFocus(() => {
      var os = Cc["@mozilla.org/observer-service;1"]
                 .getService(Ci.nsIObserverService);

      os.notifyObservers(null, 'memory-pressure', 'heap-minimize');

      // Now ask if it was.
      mm.sendAsyncMessage("test666317:wasImgDiscarded");
    }, oldTab.contentWindow);
  });

  mm.addMessageListener("test666317:wasImgDiscarded:Answer", function(message) {
    let discarded = message.data.discarded;
    ok(discarded, 'Image should be discarded.');

    // And we're done.
    gBrowser.removeTab(newTab);
    prefBranch.setBoolPref('discardable', oldDiscardingPref);
    finish();
  });

  mm.sendAsyncMessage("test666317:testPart1");
}
