waitForExplicitFinish();

let pageSource =
  '<html><body>' +
    '<img id="testImg" src="' + TESTROOT + 'big.png">' +
  '</body></html>';

let oldDiscardingPref, oldTab, newTab;
let prefBranch = Cc["@mozilla.org/preferences-service;1"]
                   .getService(Ci.nsIPrefService)
                   .getBranch('image.mem.');

function ImageDiscardObserver(result) {
  this.discard = function onDiscard(request)
  {
    result.wasDiscarded = true;
    this.synchronous = false;
  }

  this.synchronous = true;
}

function currentRequest() {
  let img = gBrowser.getBrowserForTab(newTab).contentWindow
            .document.getElementById('testImg');
  img.QueryInterface(Ci.nsIImageLoadingContent);
  return img.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
}

function isImgDecoded() {
  let request = currentRequest();
  return request.imageStatus & Ci.imgIRequest.STATUS_FRAME_COMPLETE ? true : false;
}

// Ensure that the image is decoded by drawing it to a canvas.
function forceDecodeImg() {
  let doc = gBrowser.getBrowserForTab(newTab).contentWindow.document;
  let img = doc.getElementById('testImg');
  let canvas = doc.createElement('canvas');
  let ctx = canvas.getContext('2d');
  ctx.drawImage(img, 0, 0);
}

function runAfterAsyncEvents(aCallback) {
  function handlePostMessage(aEvent) {
    if (aEvent.data == 'next') {
      window.removeEventListener('message', handlePostMessage, false);
      aCallback();
    }
  }

  window.addEventListener('message', handlePostMessage, false);

  // We'll receive the 'message' event after everything else that's currently in
  // the event queue (which is a stronger guarantee than setTimeout, because
  // setTimeout events may be coalesced). This lets us ensure that we run
  // aCallback *after* any asynchronous events are delivered.
  window.postMessage('next', '*');
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
  // Create a place to hold the result.
  var result = { wasDiscarded: false };

  // Create the discard observer.
  var observer = new ImageDiscardObserver(result);
  var scriptedObserver = Cc["@mozilla.org/image/tools;1"]
                           .getService(Ci.imgITools)
                           .createScriptedObserver(observer);

  // Clone the current imgIRequest with our new observer.
  var request = currentRequest();
  var clonedRequest = request.clone(scriptedObserver);

  // Check that the image is decoded.
  forceDecodeImg();

  // The FRAME_COMPLETE notification is delivered asynchronously, so continue
  // after we're sure it has been delivered.
  runAfterAsyncEvents(() => step3(result, scriptedObserver, clonedRequest));
}

function step3(result, scriptedObserver, clonedRequest) {
  ok(isImgDecoded(), 'Image should initially be decoded.');

  // Focus the old tab, then fire a memory-pressure notification.  This should
  // cause the decoded image in the new tab to be discarded.
  gBrowser.selectedTab = oldTab;
  var os = Cc["@mozilla.org/observer-service;1"]
             .getService(Ci.nsIObserverService);
  os.notifyObservers(null, 'memory-pressure', 'heap-minimize');

  // The DISCARD notification is delivered asynchronously, so continue after
  // we're sure it has been delivered.
  runAfterAsyncEvents(() => step4(result, scriptedObserver, clonedRequest));
}

function step4(result, scriptedObserver, clonedRequest) {
  ok(result.wasDiscarded, 'Image should be discarded.');

  // And we're done.
  gBrowser.removeTab(newTab);
  prefBranch.setBoolPref('discardable', oldDiscardingPref);
  clonedRequest.cancelAndForgetObserver(0);
  finish();
}
