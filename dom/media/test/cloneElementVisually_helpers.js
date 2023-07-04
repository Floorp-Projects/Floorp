const TEST_VIDEO_1 =
  "http://mochi.test:8888/tests/dom/media/test/bipbop_225w_175kbps.mp4";
const TEST_VIDEO_2 =
  "http://mochi.test:8888/tests/dom/media/test/pixel_aspect_ratio.mp4";
const LONG_VIDEO = "http://mochi.test:8888/tests/dom/media/test/gizmo.mp4";

/**
 * Ensure that the original <video> is prepped and ready to play
 * before starting any other tests.
 */
async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.test.video-suspend", true],
      ["media.suspend-background-video.enabled", true],
      ["media.suspend-background-video.delay-ms", 500],
      ["media.dormant-on-pause-timeout-ms", 0],
      ["media.cloneElementVisually.testing", true],
    ],
  });

  let originalVideo = document.getElementById("original");
  await setVideoSrc(originalVideo, TEST_VIDEO_1);
}

/**
 * Given a canvas, as well as a width and height of something to be
 * blitted onto that canvas, makes any necessary adjustments to the
 * canvas to work with the current display resolution.
 *
 * @param canvas (<canvas> element)
 *        The canvas to be adjusted.
 * @param width (int)
 *        The width of the image to be blitted.
 * @param height (int)
 *        The height of the image to be blitted.
 * @return CanvasRenderingContext2D (SpecialPowers wrapper)
 */
function getWrappedScaledCanvasContext(canvas, width, height) {
  let devRatio = window.devicePixelRatio || 1;
  let ctx = SpecialPowers.wrap(canvas.getContext("2d"));
  let backingRatio = ctx.webkitBackingStorePixelRatio || 1;

  let ratio = 1;
  ratio = devRatio / backingRatio;
  canvas.width = ratio * width;
  canvas.height = ratio * height;
  canvas.style.width = width + "px";
  canvas.style.height = height + "px";
  ctx.scale(ratio, ratio);

  return ctx;
}

/**
 * Given a <video> element in the DOM, figures out its location, and captures
 * a snapshot of what it's currently presenting.
 *
 * @param video (<video> element)
 * @return ImageData (SpecialPowers wrapper)
 */
function captureFrameImageData(video) {
  // Flush layout first, just in case the decoder has recently
  // updated the dimensions of the video.
  let rect = video.getBoundingClientRect();

  let width = video.videoWidth;
  let height = video.videoHeight;

  let canvas = document.createElement("canvas");
  let ctx = getWrappedScaledCanvasContext(canvas, width, height);
  ctx.drawWindow(window, rect.left, rect.top, width, height, "rgb(0,0,0)");

  return ctx.getImageData(0, 0, width, height);
}

/**
 * Given two <video> elements, captures snapshots of what they're currently
 * displaying, and asserts that they're identical.
 *
 * @param video1 (<video> element)
 *        A video element to compare against.
 * @param video2 (<video> element)
 *        A video to compare with video1.
 * @return Promise
 * @resolves
 *        Resolves as true if the two videos match.
 */
async function assertVideosMatch(video1, video2) {
  let video1Frame = captureFrameImageData(video1);
  let video2Frame = captureFrameImageData(video2);

  let left = document.getElementById("left");
  let leftCtx = getWrappedScaledCanvasContext(
    left,
    video1Frame.width,
    video1Frame.height
  );
  leftCtx.putImageData(video1Frame, 0, 0);

  let right = document.getElementById("right");
  let rightCtx = getWrappedScaledCanvasContext(
    right,
    video2Frame.width,
    video2Frame.height
  );
  rightCtx.putImageData(video2Frame, 0, 0);

  if (video1Frame.data.length != video2Frame.data.length) {
    return false;
  }

  let leftDataURL = left.toDataURL();
  let rightDataURL = right.toDataURL();

  if (leftDataURL != rightDataURL) {
    dump("Left frame: " + leftDataURL + "\n\n");
    dump("Right frame: " + rightDataURL + "\n\n");

    return false;
  }

  return true;
}

/**
 * Testing helper function that constructs a node clone of a video,
 * injects it into the DOM, and then runs an async testing function.
 * This also does the work of removing the clone before resolving.
 *
 * @param video (<video> element)
 *        The video to clone the node from.
 * @param asyncFn (async function)
 *        A test function that will be passed the new clone as its
 *        only argument.
 * @return Promise
 * @resolves
 *        When the asyncFn resolves and the clone has been removed
 *        from the DOM.
 */
async function withNewClone(video, asyncFn) {
  let clone = video.cloneNode();
  clone.id = "clone";
  clone.src = "";
  let content = document.getElementById("content");
  content.appendChild(clone);

  try {
    await asyncFn(clone);
  } finally {
    clone.remove();
  }
}

/**
 * Sets the src on a video and waits until its ready.
 *
 * @param video (<video> element)
 *        The video to set the src on.
 * @param src (string)
 *        The URL to set as the source on a video.
 * @return Promise
 * @resolves
 *        When the video fires the "canplay" event.
 */
async function setVideoSrc(video, src) {
  let promiseReady = waitForEventOnce(video, "canplay");
  video.src = src;
  await promiseReady;
}

/**
 * Returns a Promise once target emits a particular event
 * once.
 *
 * @param target (DOM node)
 *        The target to monitor for the event.
 * @param event (string)
 *        The name of the event to wait for.
 * @return Promise
 * @resolves
 *        When the event fires, and resolves to the event.
 */
function waitForEventOnce(target, event) {
  return new Promise(resolve => {
    target.addEventListener(event, resolve, { once: true });
  });
}

/**
 * Polls the video debug data as a hacky way of knowing when
 * when the decoders have shut down.
 *
 * @param video (<video> element)
 * @return Promise
 * @resolves
 *        When the decoder has shut down.
 */
function waitForShutdownDecoder(video) {
  return SimpleTest.promiseWaitForCondition(async () => {
    let readerData = await SpecialPowers.wrap(video).mozRequestDebugInfo();
    return readerData.decoder.reader.audioDecoderName == "shutdown";
  }, "Video decoder should eventually shut down.");
}

/**
 * Ensures that both hiding and pausing the video causes the
 * video to suspend and make dormant its decoders, respectively.
 *
 * @param video (<video element)
 */
async function ensureVideoSuspendable(video) {
  video = SpecialPowers.wrap(video);

  ok(!video.hasSuspendTaint(), "Should be suspendable");

  // First, we'll simulate putting the video in the background by
  // making it invisible.
  let suspendPromise = waitForEventOnce(video, "mozentervideosuspend");
  video.setVisible(false);
  await suspendPromise;
  ok(true, "Suspended after the video was made invisible.");
  video.setVisible(true);

  ok(!video.hasSuspendTaint(), "Should still be suspendable.");

  // Next, we'll pause the video.
  await video.pause();
  await waitForShutdownDecoder(video);
  ok(true, "Shutdown decoder after the video was paused.");
  await video.play();
}
