importScripts('utils_bug743615.js');

self.onmessage = function onMessage(evt) {
  // Check the pattern that was sent.
  var imageData = evt.data.imageData;
  var pattern = evt.data.pattern;
  var statusMessage = checkPattern(imageData, pattern)
                       ? 'PASS' : 'Got corrupt typed array in worker';

  // Check against the interface object.
  if (!(imageData instanceof ImageData))
    statusMessage += ", Bad interface object in worker";

  // Check the getters.
  if (imageData.width * imageData.height != imageData.data.length / 4) {
    statusMessage += ", Bad ImageData getters in worker: "
    statusMessage += [imageData.width, imageData.height].join(', ');
  }

  // Make sure that writing to .data is a no-op when not in strict mode.
  var origData = imageData.data;
  var threw = false;
  try {
    imageData.data = [];
    imageData.width = 2;
    imageData.height = 2;
  } catch(e) { threw = true; }
  if (threw || imageData.data !== origData)
    statusMessage = statusMessage + ", Should silently ignore sets";



  // Send back a new pattern.
  pattern = makePattern(imageData.data.length, 99, 2);
  setPattern(imageData, pattern);
  self.postMessage({ statusMessage: statusMessage, imageData: imageData,
                     pattern: pattern });
}
