/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
var currentTest;
var gIsRefImageLoaded = false;
const gShouldOutputDebugInfo = false;

function pollForSuccess()
{
  if (!currentTest.isTestFinished) {
    if (!currentTest.reusingReferenceImage || (currentTest.reusingReferenceImage
        && gRefImageLoaded)) {
      currentTest.checkImage();
    }

    setTimeout(pollForSuccess, currentTest.pollFreq);
  }
};

function referencePoller()
{
  currentTest.takeReferenceSnapshot();
}

function reuseImageCallback()
{
  gIsRefImageLoaded = true;
}

function failTest()
{
  if (currentTest.isTestFinished || currentTest.closeFunc) {
    return;
  }

  ok(false, "timing out after " + currentTest.timeout + "ms.  "
     + "Animated image still doesn't look correct, after poll #"
     + currentTest.pollCounter);
  currentTest.wereFailures = true;

  if (currentTest.currentSnapshotDataURI) {
    currentTest.outputDebugInfo("Snapshot #" + currentTest.pollCounter,
                                "snapNum" + currentTest.pollCounter,
                                currentTest.currentSnapshotDataURI);
  }

  currentTest.enableDisplay(document.getElementById(currentTest.debugElementId));

  currentTest.cleanUpAndFinish();
};

/**
 * Create a new AnimationTest object.
 *
 * @param pollFreq The amount of time (in ms) to wait between consecutive
 *        snapshots if the reference image and the test image don't match.
 * @param timeout The total amount of time (in ms) to wait before declaring the
 *        test as failed.
 * @param referenceElementId The id attribute of the reference image element, or
 *        the source of the image to change to, once the reference snapshot has
 *        been successfully taken. This latter option could be used if you don't
 *        want the image to become invisible at any time during the test.
 * @param imageElementId The id attribute of the test image element.
 * @param debugElementId The id attribute of the div where links should be
 *        appended if the test fails.
 * @param cleanId The id attribute of the div or element to use as the 'clean'
 *        test. This element is only enabled when we are testing to verify that
 *        the reference image has been loaded. It can be undefined.
 * @param srcAttr The location of the source of the image, for preloading. This
 *        is usually not required, but it useful for preloading reference
 *        images.
 * @param xulTest A boolean value indicating whether or not this is a XUL test
 *        (uses hidden=true/false rather than display: none to hide/show
 *        elements).
 * @param closeFunc A function that should be called when this test is finished.
 *        If null, then cleanUpAndFinish() will be called. This can be used to
 *        chain tests together, so they are all finished exactly once.
 * @returns {AnimationTest}
 */
function AnimationTest(pollFreq, timeout, referenceElementId, imageElementId,
                       debugElementId, cleanId, srcAttr, xulTest, closeFunc)
{
  // We want to test the cold loading behavior, so clear cache in case an
  // earlier test got our image in there already.
  clearImageCache();

  this.wereFailures = false;
  this.pollFreq = pollFreq;
  this.timeout = timeout;
  this.imageElementId = imageElementId;
  this.referenceElementId = referenceElementId;

  if (!document.getElementById(referenceElementId)) {
    // In this case, we're assuming the user passed in a string that
    // indicates the source of the image they want to change to,
    // after the reference image has been taken.
    this.reusingImageAsReference = true;
  }

  this.srcAttr = srcAttr;
  this.debugElementId = debugElementId;
  this.referenceSnapshot = ""; // value will be set in takeReferenceSnapshot()
  this.pollCounter = 0;
  this.isTestFinished = false;
  this.numRefsTaken = 0;
  this.blankWaitTime = 0;

  this.cleanId = cleanId ? cleanId : '';
  this.xulTest = xulTest ? xulTest : '';
  this.closeFunc = closeFunc ? closeFunc : '';
};

AnimationTest.prototype.preloadImage = function()
{
  if (this.srcAttr) {
    this.myImage = new Image();
    this.myImage.onload = function() { currentTest.continueTest(); };
    this.myImage.src = this.srcAttr;
  } else {
    this.continueTest();
  }
};

AnimationTest.prototype.outputDebugInfo = function(message, id, dataUri)
{
  if (!gShouldOutputDebugInfo) {
    return;
  }
  var debugElement = document.getElementById(this.debugElementId);
  var newDataUriElement = document.createElement("a");
  newDataUriElement.setAttribute("id", id);
  newDataUriElement.setAttribute("href", dataUri);
  newDataUriElement.appendChild(document.createTextNode(message));
  debugElement.appendChild(newDataUriElement);
  var brElement = document.createElement("br");
  debugElement.appendChild(brElement);
  todo(false, "Debug (" + id + "): " + message + " " + dataUri);
};

AnimationTest.prototype.isFinished = function()
{
  return this.isTestFinished;
};

AnimationTest.prototype.takeCleanSnapshot = function()
{
  var cleanElement;
  if (this.cleanId) {
    cleanElement = document.getElementById(this.cleanId);
  }

  // Enable clean page comparison element
  if (cleanElement) {
    this.enableDisplay(cleanElement);
  }

  // Take a snapshot of the initial (clean) page
  this.cleanSnapshot = snapshotWindow(window, false);

  // Disable the clean page comparison element
  if (cleanElement) {
    this.disableDisplay(cleanElement);
  }

  var dataString1 = "Clean Snapshot";
  this.outputDebugInfo(dataString1, 'cleanSnap',
                       this.cleanSnapshot.toDataURL());
};

AnimationTest.prototype.takeBlankSnapshot = function()
{
  // Take a snapshot of the initial (essentially blank) page
  this.blankSnapshot = snapshotWindow(window, false);

  var dataString1 = "Initial Blank Snapshot";
  this.outputDebugInfo(dataString1, 'blank1Snap',
                       this.blankSnapshot.toDataURL());
};

/**
 * Begin the AnimationTest. This will utilize the information provided in the
 * constructor to invoke a mochitest on animated images. It will automatically
 * fail if allowed to run past the timeout. This will attempt to preload an
 * image, if applicable, and then asynchronously call continueTest(), or if not
 * applicable, synchronously trigger a call to continueTest().
 */
AnimationTest.prototype.beginTest = function()
{
  SimpleTest.waitForExplicitFinish();

  currentTest = this;
  this.preloadImage();
};

/**
 * This is the second part of the test. It is triggered (eventually) from
 * beginTest() either synchronously or asynchronously, as an image load
 * callback.
 */
AnimationTest.prototype.continueTest = function()
{
  // In case something goes wrong, fail earlier than mochitest timeout,
  // and with more information.
  setTimeout(failTest, this.timeout);

  if (!this.reusingImageAsReference) {
    this.disableDisplay(document.getElementById(this.imageElementId));
  }

  this.takeReferenceSnapshot();
  this.setupPolledImage();
  SimpleTest.executeSoon(pollForSuccess);
};

AnimationTest.prototype.setupPolledImage = function ()
{
  // Make sure the image is visible
  if (!this.reusingImageAsReference) {
    this.enableDisplay(document.getElementById(this.imageElementId));
    var currentSnapshot = snapshotWindow(window, false);
    var result = compareSnapshots(currentSnapshot,
                                  this.referenceSnapshot, true);

    this.currentSnapshotDataURI = currentSnapshot.toDataURL();

    if (result[0]) {
      // SUCCESS!
      ok(true, "Animated image looks correct, at poll #"
         + this.pollCounter);

      this.cleanUpAndFinish();
    }
  } else {
    if (!gIsRefImageLoaded) {
      this.myImage = new Image();
      this.myImage.onload = reuseImageCallback;
      document.getElementById(this.imageElementId).setAttribute('src',
        this.referenceElementId);
    }
  }
}

AnimationTest.prototype.checkImage = function ()
{
  if (this.isTestFinished) {
    return;
  }

  this.pollCounter++;

  // We need this for some tests, because we need to force the
  // test image to be visible.
  if (!this.reusingImageAsReference) {
    this.enableDisplay(document.getElementById(this.imageElementId));
  }

  var currentSnapshot = snapshotWindow(window, false);
  var result = compareSnapshots(currentSnapshot, this.referenceSnapshot, true);

  this.currentSnapshotDataURI = currentSnapshot.toDataURL();

  if (result[0]) {
    // SUCCESS!
    ok(true, "Animated image looks correct, at poll #"
       + this.pollCounter);

    this.cleanUpAndFinish();
  }
};

AnimationTest.prototype.takeReferenceSnapshot = function ()
{
  this.numRefsTaken++;

  // Test to make sure the reference image doesn't match a clean snapshot
  if (!this.cleanSnapshot) {
    this.takeCleanSnapshot();
  }

  // Used later to verify that the reference div disappeared
  if (!this.blankSnapshot) {
    this.takeBlankSnapshot();
  }

  if (this.reusingImageAsReference) {
    // Show reference elem (which is actually our image), & take a snapshot
    var referenceElem = document.getElementById(this.imageElementId);
    this.enableDisplay(referenceElem);

    this.referenceSnapshot = snapshotWindow(window, false);

    var snapResult = compareSnapshots(this.cleanSnapshot,
                                      this.referenceSnapshot, false);
    if (!snapResult[0]) {
      if (this.blankWaitTime > 2000) {
        // if it took longer than two seconds to load the image, we probably
        // have a problem.
        this.wereFailures = true;
        ok(snapResult[0],
           "Reference snapshot shouldn't match clean (non-image) snapshot");
      } else {
        this.blankWaitTime += currentTest.pollFreq;
        // let's wait a bit and see if it clears up
        setTimeout(referencePoller, currentTest.pollFreq);
        return;
      }
    }

    ok(snapResult[0],
       "Reference snapshot shouldn't match clean (non-image) snapshot");

    var dataString = "Reference Snapshot #" + this.numRefsTaken;
    this.outputDebugInfo(dataString, 'refSnapId',
                         this.referenceSnapshot.toDataURL());
  } else {
    // Make sure the animation section is hidden
    this.disableDisplay(document.getElementById(this.imageElementId));

    // Show reference div, & take a snapshot
    var referenceDiv = document.getElementById(this.referenceElementId);
    this.enableDisplay(referenceDiv);

    this.referenceSnapshot = snapshotWindow(window, false);
    var snapResult = compareSnapshots(this.cleanSnapshot,
                                      this.referenceSnapshot, false);
    if (!snapResult[0]) {
      if (this.blankWaitTime > 2000) {
        // if it took longer than two seconds to load the image, we probably
        // have a problem.
        this.wereFailures = true;
        ok(snapResult[0],
           "Reference snapshot shouldn't match clean (non-image) snapshot");
      } else {
        this.blankWaitTime += 20;
        // let's wait a bit and see if it clears up
        setTimeout(referencePoller, 20);
        return;
      }
    }

    ok(snapResult[0],
       "Reference snapshot shouldn't match clean (non-image) snapshot");

    var dataString = "Reference Snapshot #" + this.numRefsTaken;
    this.outputDebugInfo(dataString, 'refSnapId',
                         this.referenceSnapshot.toDataURL());

    // Re-hide reference div, and take another snapshot to be sure it's gone
    this.disableDisplay(referenceDiv);
    this.testBlankCameBack();
  }
};

AnimationTest.prototype.enableDisplay = function(element)
{
  if (!element) {
    return;
  }

  if (!this.xulTest) {
    element.style.display = '';
  } else {
    element.setAttribute('hidden', 'false');
  }
};

AnimationTest.prototype.disableDisplay = function(element)
{
  if (!element) {
    return;
  }

  if (!this.xulTest) {
    element.style.display = 'none';
  } else {
    element.setAttribute('hidden', 'true');
  }
};

AnimationTest.prototype.testBlankCameBack = function()
{
  var blankSnapshot2 = snapshotWindow(window, false);
  var result = compareSnapshots(this.blankSnapshot, blankSnapshot2, true);
  ok(result[0], "Reference image should disappear when it becomes display:none");

  if (!result[0]) {
    this.wereFailures = true;
    var dataString = "Second Blank Snapshot";
    this.outputDebugInfo(dataString, 'blank2SnapId', result[2]);
  }
};

AnimationTest.prototype.cleanUpAndFinish = function ()
{
  // On the off chance that failTest and checkImage are triggered
  // back-to-back, use a flag to prevent multiple calls to SimpleTest.finish.
  if (this.isTestFinished) {
    return;
  }

  this.isTestFinished = true;

  // Call our closing function, if one exists
  if (this.closeFunc) {
    this.closeFunc();
    return;
  }

  if (this.wereFailures) {
    document.getElementById(this.debugElementId).style.display = 'block';
  }

  SimpleTest.finish();
  document.getElementById(this.debugElementId).style.display = "";
};
