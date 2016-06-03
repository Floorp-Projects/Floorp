importScripts("imagebitmap_extensions_data.js");
importScripts("imagebitmap_extensions.js");

var gGroundTruthImageData;
var gImageData;
var gImageBitmap;
var gPNGBlob;
var gJPEGBlob;

onmessage = function(event) {
  if (event.data.type == "setSources") {
    gGroundTruthImageData = event.data.groundTruthImageData;
    gImageData = event.data.imageData;
    gImageBitmap = event.data.imageBitmap;
    gPNGBlob = event.data.pngBlob;
    gJPEGBlob = event.data.jpegBlob;

    ok(!!gGroundTruthImageData, "Get gGroundTruthImageData!");
    ok(!!gImageData, "Get gImageData!");
    ok(!!gImageBitmap, "Get gImageBitmap!");
    ok(!!gPNGBlob, "Get gPNGBlob!");
    ok(!!gJPEGBlob, "Get gJPEGBlob!");

    runTests();
  }
};

function ok(expect, msg) {
  postMessage({"type": "status", status: !!expect, msg: msg});
}

function runTests() {
  testExceptions().
  then(testColorConversions()).
  then( function() { return Promise.all([testAccessing_randomTest("ImageData", gImageData, 0),
                                         testAccessing_randomTest("ImageBitmap", gImageBitmap, 0),
                                         testAccessing_randomTest("PNG", gPNGBlob, 0),
                                         testAccessing_randomTest("JPEG", gJPEGBlob, 10) // JPEG loses information
                                        ]); }).
  then( function() { return Promise.all([testCreateFromArrayBffer_randomTest("ImageData", gImageData, 0),
                                         testCreateFromArrayBffer_randomTest("ImageBitmap", gImageBitmap, 0),
                                         testCreateFromArrayBffer_randomTest("PNG", gPNGBlob, 0),
                                         testCreateFromArrayBffer_randomTest("JPEG", gJPEGBlob, 10) // JPEG loses information
                                        ]); }).
  then(function() { return testInvalidAccess([gImageData, gImageBitmap, gPNGBlob, gJPEGBlob]); } ).
  then(function() {postMessage({"type": "finish"});}, function(ev) { failed(ev); postMessage({"type": "finish"}); });
}