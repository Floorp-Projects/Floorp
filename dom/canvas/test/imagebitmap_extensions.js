function failed(ex) {
  ok(false, "Promise failure: " + ex);
}

function isPixel(sourceType, bitmapFormat, imageData, bitmapImageData, x, y, tolerance) {
  if (imageData.width != bitmapImageData.width ||
    imageData.height != bitmapImageData.height) {
    ok(false, "Wrong dimension");
  }

  var index = 4 * (y * imageData.width + x);

  var pr = imageData.data[index+0],
      pg = imageData.data[index+1],
      pb = imageData.data[index+2],
      pa = imageData.data[index+3];

  if (bitmapFormat == "RGBA32" || bitmapFormat == "RGBX32") {
    var bpr = bitmapImageData.data[index+0],
        bpg = bitmapImageData.data[index+1],
        bpb = bitmapImageData.data[index+2],
        bpa = bitmapImageData.data[index+3];
  }
  else if (bitmapFormat == "BGRA32" || bitmapFormat == "BGRX32") {
    var bpb = bitmapImageData.data[index+0],
        bpg = bitmapImageData.data[index+1],
        bpr = bitmapImageData.data[index+2],
        bpa = bitmapImageData.data[index+3];
  }
  else {
    // format might be one of the followings: "R5G6B5", "A8", "YUV", ""
    ok(false, "Not supported ImageFormat: " + bitmapFormat);
  }

  ok(pr - tolerance <= bpr && bpr <= pr + tolerance &&
     pg - tolerance <= bpg && bpg <= pg + tolerance &&
     pb - tolerance <= bpb && bpb <= pb + tolerance &&
     pa - tolerance <= bpa && bpa <= pa + tolerance,
     "pixel[" + x + "][" + y + "]: " + sourceType + " is "+pr+","+pg+","+pb+","+pa+"; ImageBitmap is "+ bpr + "," + bpg + "," + bpb + "," + bpa);
}

function promiseThrows(p, name) {
  var didThrow;
  return p.then(function() { didThrow = false; },
                function() { didThrow = true; })
          .then(function() { ok(didThrow, name); });
}

function testExceptions() {

  function callMappedDataLengthWithUnsupportedFormat(format) {
    return new Promise(function(resolve, reject) {
      var p = createImageBitmap(gImageData);
      p.then(
        function(bitmap) {
          try {
            bitmap.mappedDataLength(format);
            resolve();
          }
          catch (e) {
            reject(e);
          }
        },
        function() { resolve(); });
    });
  }

  return Promise.all([
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("RGB24"), "[Exception] RGB24 is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("BGR24"), "[Exception] BGR24 is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("GRAY8"), "[Exception] GRAY8 is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("YUV444P"), "[Exception] GRYUV444PAY is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("YUV422P"), "[Exception] YUV422P is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("YUV420SP_NV12"), "[Exception] YUV420SP_NV12 is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("YUV420SP_NV21"), "[Exception] YUV420SP_NV21 is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("HSV"), "[Exception] HSV is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("Lab"), "[Exception] Lab is not supported yet."),
    promiseThrows(callMappedDataLengthWithUnsupportedFormat("DEPTH"), "[Exception] DEPTH is not supported yet.")
  ]);
}

// Create an ImageBitmap, _bitmap_, from the _source_.
// Read the underlying data of _bitmap_ into _bitmapBuffer_.
// Compare the _bitmapBuffer_ with gGroundTruthImageData.
function testAccessing_randomTest(sourceType, source, duration) {
  return new Promise(function(resolve, reject) {
    var p  = createImageBitmap(source);
    p.then(
      function(bitmap) {
        bitmapFormat = "RGBA32";
        var bitmapBufferLength = bitmap.mappedDataLength(bitmapFormat);

        var bitmapBuffer = new ArrayBuffer(bitmapBufferLength);
        var bitmapBufferView = new Uint8ClampedArray(bitmapBuffer, 0, bitmapBufferLength);
        var promise = bitmap.mapDataInto(bitmapFormat, bitmapBuffer, 0);
        promise.then(
          function(bitmapPixelLayout) {
            // Prepare.
            bitmapImageData = new ImageData(bitmapBufferView, bitmap.width, bitmap.height);

            // Test.
            for (var t = 0; t < 50; ++t) {
              var randomX = Math.floor(Math.random() * 240);
              var randomY = Math.floor(Math.random() * 175);
              isPixel(sourceType, "RGBA32", gGroundTruthImageData, bitmapImageData, randomX, randomY, duration);
            }

            resolve();
          },
          function(ev) { failed(ev); reject(); });
      },
      function(ev) { failed(ev); reject(); });
  });
}

// Create an ImageBitmap, _bitmap_, from the _source_.
// Read the underlying data of _bitmap_ into _bitmapBuffer_.
// Create another ImageBitmap, _bitmap2_, from _bitmapBuffer_.
// Read the underlying data of _bitmap2_ into _bitmapBuffer2_.
// Compare the _bitmapBuffer2_ with gGroundTruthImageData.
function testCreateFromArrayBffer_randomTest(sourceType, source, duration) {
  return new Promise(function(resolve, reject) {
    var p  = createImageBitmap(source);
    p.then(
      function(bitmap) {
        bitmapFormat = "RGBA32";
        var bitmapBufferLength = bitmap.mappedDataLength(bitmapFormat);

        var bitmapBuffer = new ArrayBuffer(bitmapBufferLength);
        var bitmapBufferView = new Uint8ClampedArray(bitmapBuffer, 0, bitmapBufferLength);
        var promiseMapDataInto = bitmap.mapDataInto(bitmapFormat, bitmapBuffer, 0);
        promiseMapDataInto.then(
          function(bitmapPixelLayout) {
            // Create a new ImageBitmap from an ArrayBuffer.
            var p2 = createImageBitmap(bitmapBufferView,
                                       0,
                                       bitmapBufferLength,
                                       bitmapFormat,
                                       bitmapPixelLayout);

            p2.then(
              function(bitmap2) {
                bitmapFormat2 = "RGBA32";
                var bitmapBufferLength2 = bitmap2.mappedDataLength(bitmapFormat2);

                var bitmapBuffer2 = new ArrayBuffer(bitmapBufferLength2);
                var bitmapBufferView2 = new Uint8ClampedArray(bitmapBuffer2, 0, bitmapBufferLength2);
                var promise2 = bitmap2.mapDataInto(bitmapFormat2, bitmapBuffer2, 0);
                promise2.then(
                  function(bitmapPixelLayout2) {
                    // Prepare.
                    var bitmapImageData2 = new ImageData(bitmapBufferView2, bitmap2.width, bitmap2.height);

                    // Test.
                    for (var t = 0; t < 50; ++t) {
                      var randomX = Math.floor(Math.random() * 240);
                      var randomY = Math.floor(Math.random() * 175);
                      isPixel(sourceType, "RGBA32", gGroundTruthImageData, bitmapImageData2, randomX, randomY, duration);
                    }

                    resolve();
                  },
                  function(ev) { failed(ev); reject(); });
              },
              function(ev) { console.log("p2 rejected!"); failed(ev); reject(); });
          },
          function(ev) { console.log("promiseMapDataInto rejected!"); failed(ev); reject(); });
      },
      function(ev) { failed(ev); reject(); });
  });
}