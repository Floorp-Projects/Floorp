var gImage;
var gVideo;
var gCanvas;
var gCtx;
var gImageData;
var gImageBitmap;
var gPNGBlob;
var gJPEGBlob;

var gGroundTruthImageData;

function prepareSources() {
  gVideo = document.createElement("video");
  gVideo.src = "http://example.com/tests/dom/canvas/test/crossorigin/video.sjs?name=tests/dom/media/test/320x240.ogv&type=video/ogg&cors=anonymous";
  gVideo.crossOrigin = "anonymous";
  gVideo.autoplay = "true"


  gCanvas = document.createElement("canvas");
  gCtx = gCanvas.getContext("2d");

  var resolver;
  var promise = new Promise(function(resolve, reject) {
    resolver = resolve;
  });

  // Prepare video.
  gVideo.onloadeddata = function() {
    ok(gVideo, "[Prepare Sources] gVideo is ok.");

    // Prepare canvas.
    gCanvas.width = gVideo.videoWidth;
    gCanvas.height = gVideo.videoHeight;
    gCtx.drawImage(gVideo, 0, 0);
    ok(gCanvas, "[Prepare Sources] gCanvas is ok.");
    ok(gCtx, "[Prepare Sources] gCtx is ok.");

    // Prepare gGroundTruthImageData.
    gGroundTruthImageData = gCtx.getImageData(0, 0, gCanvas.width, gCanvas.height);
    ok(gGroundTruthImageData, "[Prepare Sources] gGroundTruthImageData is ok.");

    // Prepare image.
    gImage = document.createElement("img");
    gImage.src = gCanvas.toDataURL();
    var resolverImage;
    var promiseImage = new Promise(function(resolve, reject) {
      resolverImage = resolve;
    });
    gImage.onload = function() {
      resolverImage(true);
    }

    // Prepare ImageData.
    gImageData = gCtx.getImageData(0, 0, gCanvas.width, gCanvas.height);
    ok(gImageData, "[Prepare Sources] gImageData is ok.");

    // Prepapre PNG Blob.
    var promisePNGBlob = new Promise(function(resolve, reject) {
      gCanvas.toBlob(function(blob) {
        gPNGBlob = blob;
        ok(gPNGBlob, "[Prepare Sources] gPNGBlob is ok.");
        resolve(true);
      });
    });

    // Prepare JPEG Blob.
    var promiseJPEGBlob = new Promise(function(resolve, reject) {
      gCanvas.toBlob(function(blob) {
        gJPEGBlob = blob;
        ok(gJPEGBlob, "[Prepare Sources] gJPEGBlob is ok.");
        resolve(true);
      }, "image/jpeg", 1.00);
    });

    // Prepare ImageBitmap.
    var promiseImageBitmap = new Promise(function(resolve, reject) {
      var p = createImageBitmap(gCanvas);
      p.then(function(bitmap) {
        gImageBitmap = bitmap;
        ok(gImageBitmap, "[Prepare Sources] gImageBitmap is ok.");
        resolve(true);
      });
    });

    resolver(Promise.all([
      promiseImage,
      promisePNGBlob,
      promiseJPEGBlob,
      promiseImageBitmap
    ]))
  }

  return promise;
}