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
  return Promise.all([
    promiseThrows(testColorConversion("GRAY8", "RGBA32", undefined, true), "[Exception] Cannot convert from GRAY8 to RGBA32"),
    promiseThrows(testColorConversion("GRAY8", "BGRA32", undefined, true), "[Exception] Cannot convert from GRAY8 to BGRA32"),
    promiseThrows(testColorConversion("GRAY8", "RGB24", undefined, true), "[Exception] Cannot convert from GRAY8 to RGB24"),
    promiseThrows(testColorConversion("GRAY8", "BGR24", undefined, true), "[Exception] Cannot convert from GRAY8 to BGR24"),
    promiseThrows(testColorConversion("GRAY8", "YUV444P", undefined, true), "[Exception] Cannot convert from GRAY8 to YUV444P"),
    promiseThrows(testColorConversion("GRAY8", "YUV422P", undefined, true), "[Exception] Cannot convert from GRAY8 to YUV422P"),
    promiseThrows(testColorConversion("GRAY8", "YUV420P", undefined, true), "[Exception] Cannot convert from GRAY8 to YUV420P"),
    promiseThrows(testColorConversion("GRAY8", "YUV420SP_NV12", undefined, true), "[Exception] Cannot convert from GRAY8 to YUV420SP_NV12"),
    promiseThrows(testColorConversion("GRAY8", "YUV420SP_NV21", undefined, true), "[Exception] Cannot convert from GRAY8 to YUV420SP_NV21"),
    promiseThrows(testColorConversion("GRAY8", "HSV", undefined, true), "[Exception] Cannot convert from GRAY8 to HSV"),
    promiseThrows(testColorConversion("GRAY8", "Lab", undefined, true), "[Exception] Cannot convert from GRAY8 to Lab"),
    promiseThrows(testColorConversion("GRAY8", "DEPTH", undefined, true), "[Exception] Cannot convert from GRAY8 to DEPTH"),

    promiseThrows(testColorConversion("DEPTH", "RGBA32", undefined, true), "[Exception] Cannot convert from DEPTH to RGBA32"),
    promiseThrows(testColorConversion("DEPTH", "BGRA32", undefined, true), "[Exception] Cannot convert from DEPTH to BGRA32"),
    promiseThrows(testColorConversion("DEPTH", "RGB24", undefined, true), "[Exception] Cannot convert from DEPTH to RGB24"),
    promiseThrows(testColorConversion("DEPTH", "BGR24", undefined, true), "[Exception] Cannot convert from DEPTH to BGR24"),
    promiseThrows(testColorConversion("DEPTH", "GRAY8", undefined, true), "[Exception] Cannot convert from DEPTH to GRAY8"),
    promiseThrows(testColorConversion("DEPTH", "YUV444P", undefined, true), "[Exception] Cannot convert from DEPTH to YUV444P"),
    promiseThrows(testColorConversion("DEPTH", "YUV422P", undefined, true), "[Exception] Cannot convert from DEPTH to YUV422P"),
    promiseThrows(testColorConversion("DEPTH", "YUV420P", undefined, true), "[Exception] Cannot convert from DEPTH to YUV420P"),
    promiseThrows(testColorConversion("DEPTH", "YUV420SP_NV12", undefined, true), "[Exception] Cannot convert from DEPTH to YUV420SP_NV12"),
    promiseThrows(testColorConversion("DEPTH", "YUV420SP_NV21", undefined, true), "[Exception] Cannot convert from DEPTH to YUV420SP_NV21"),
    promiseThrows(testColorConversion("DEPTH", "HSV", undefined, true), "[Exception] Cannot convert from DEPTH to HSV"),
    promiseThrows(testColorConversion("DEPTH", "Lab", undefined, true), "[Exception] Cannot convert from DEPTH to Lab"),

    promiseThrows(testColorConversion("RGBA32", "DEPTH", undefined, true), "[Exception] Cannot convert from RGBA32 to DEPTH"),
    promiseThrows(testColorConversion("BGRA32", "DEPTH", undefined, true), "[Exception] Cannot convert from BGRA32 to DEPTH"),
    promiseThrows(testColorConversion("RGB24", "DEPTH", undefined, true), "[Exception] Cannot convert from RGB24 to DEPTH"),
    promiseThrows(testColorConversion("BGR24", "DEPTH", undefined, true), "[Exception] Cannot convert from BGR24 to DEPTH"),
    promiseThrows(testColorConversion("YUV444P", "DEPTH", undefined, true), "[Exception] Cannot convert from YUV444P to DEPTH"),
    promiseThrows(testColorConversion("YUV422P", "DEPTH", undefined, true), "[Exception] Cannot convert from YUV422P to DEPTH"),
    promiseThrows(testColorConversion("YUV420P", "DEPTH", undefined, true), "[Exception] Cannot convert from YUV420P to DEPTH"),
    promiseThrows(testColorConversion("YUV420SP_NV12", "DEPTH", undefined, true), "[Exception] Cannot convert from YUV420SP_NV12 to DEPTH"),
    promiseThrows(testColorConversion("YUV420SP_NV21", "DEPTH", undefined, true), "[Exception] Cannot convert from YUV420SP_NV21 to DEPTH"),
    promiseThrows(testColorConversion("HSV", "DEPTH", undefined, true), "[Exception] Cannot convert from HSV to DEPTH"),
    promiseThrows(testColorConversion("Lab", "DEPTH", undefined, true), "[Exception] Cannot convert from Lab to DEPTH"),
  ]);
}

function testInvalidAccess(sources) {

  function callMapDataIntoWithImageBitmapCroppedOutSideOfTheSourceImage(source) {
    return new Promise(function(resolve, reject) {
      var p = createImageBitmap(source, -1, -1, 2, 2);
      p.then(
        function(bitmap) {
          var format = bitmap.findOptimalFormat();
          var length = bitmap.mappedDataLength(format);
          var buffer = new ArrayBuffer(length);
          bitmap.mapDataInto(format, buffer, 0).then(
            function(layout) { resolve(); },
            function(error) { reject(error); }
          );
        },
        function() { resolve(); });
    });
  };

  var testCases = sources.map( function(source) {
    return promiseThrows(callMapDataIntoWithImageBitmapCroppedOutSideOfTheSourceImage(source),
                         "[Exception] mapDataInto() should throw with transparent black."); });

  return Promise.all(testCases);
}

function testColorConversions() {
  return Promise.all([// From RGBA32
                      testColorConversion("RGBA32", "RGBA32"),
                      testColorConversion("RGBA32", "BGRA32"),
                      testColorConversion("RGBA32", "RGB24"),
                      testColorConversion("RGBA32", "BGR24"),
                      testColorConversion("RGBA32", "GRAY8"),
                      testColorConversion("RGBA32", "YUV444P"),
                      testColorConversion("RGBA32", "YUV422P"),
                      testColorConversion("RGBA32", "YUV420P"),
                      testColorConversion("RGBA32", "YUV420SP_NV12"),
                      testColorConversion("RGBA32", "YUV420SP_NV21"),
                      testColorConversion("RGBA32", "HSV", 0.01),
                      testColorConversion("RGBA32", "Lab", 0.5),

                      // From BGRA32
                      testColorConversion("BGRA32", "RGBA32"),
                      testColorConversion("BGRA32", "BGRA32"),
                      testColorConversion("BGRA32", "RGB24"),
                      testColorConversion("BGRA32", "BGR24"),
                      testColorConversion("BGRA32", "GRAY8"),
                      testColorConversion("BGRA32", "YUV444P", 3),
                      testColorConversion("BGRA32", "YUV422P"),
                      testColorConversion("BGRA32", "YUV420P"),
                      testColorConversion("BGRA32", "YUV420SP_NV12"),
                      testColorConversion("BGRA32", "YUV420SP_NV21"),
                      testColorConversion("BGRA32", "HSV", 0.01),
                      testColorConversion("BGRA32", "Lab", 0.5),

                      // From RGB24
                      testColorConversion("RGB24", "RGBA32"),
                      testColorConversion("RGB24", "BGRA32"),
                      testColorConversion("RGB24", "RGB24"),
                      testColorConversion("RGB24", "BGR24"),
                      testColorConversion("RGB24", "GRAY8"),
                      testColorConversion("RGB24", "YUV444P"),
                      testColorConversion("RGB24", "YUV422P"),
                      testColorConversion("RGB24", "YUV420P"),
                      testColorConversion("RGB24", "YUV420SP_NV12"),
                      testColorConversion("RGB24", "YUV420SP_NV21"),
                      testColorConversion("RGB24", "HSV", 0.01),
                      testColorConversion("RGB24", "Lab", 0.5),

                      // From BGR24
                      testColorConversion("BGR24", "RGBA32"),
                      testColorConversion("BGR24", "BGRA32"),
                      testColorConversion("BGR24", "RGB24"),
                      testColorConversion("BGR24", "BGR24"),
                      testColorConversion("BGR24", "GRAY8"),
                      testColorConversion("BGR24", "YUV444P"),
                      testColorConversion("BGR24", "YUV422P"),
                      testColorConversion("BGR24", "YUV420P"),
                      testColorConversion("BGR24", "YUV420SP_NV12"),
                      testColorConversion("BGR24", "YUV420SP_NV21"),
                      testColorConversion("BGR24", "HSV", 0.01),
                      testColorConversion("BGR24", "Lab", 0.5),

                      // From YUV444P
                      testColorConversion("YUV444P", "RGBA32"),
                      testColorConversion("YUV444P", "BGRA32"),
                      testColorConversion("YUV444P", "RGB24"),
                      testColorConversion("YUV444P", "BGR24"),
                      testColorConversion("YUV444P", "GRAY8"),
                      testColorConversion("YUV444P", "YUV444P"),
                      testColorConversion("YUV444P", "YUV422P", 3),
                      testColorConversion("YUV444P", "YUV420P", 3),
                      testColorConversion("YUV444P", "YUV420SP_NV12", 3),
                      testColorConversion("YUV444P", "YUV420SP_NV21", 3),
                      testColorConversion("YUV444P", "HSV", 0.01),
                      testColorConversion("YUV444P", "Lab", 0.01),

                      // From YUV422P
                      testColorConversion("YUV422P", "RGBA32"),
                      testColorConversion("YUV422P", "BGRA32"),
                      testColorConversion("YUV422P", "RGB24"),
                      testColorConversion("YUV422P", "BGR24"),
                      testColorConversion("YUV422P", "GRAY8"),
                      testColorConversion("YUV422P", "YUV444P", 3),
                      testColorConversion("YUV422P", "YUV422P"),
                      testColorConversion("YUV422P", "YUV420P"),
                      testColorConversion("YUV422P", "YUV420SP_NV12"),
                      testColorConversion("YUV422P", "YUV420SP_NV21"),
                      testColorConversion("YUV422P", "HSV", 0.01),
                      testColorConversion("YUV422P", "Lab", 0.01),

                      // From YUV420P
                      testColorConversion("YUV420P", "RGBA32"),
                      testColorConversion("YUV420P", "BGRA32"),
                      testColorConversion("YUV420P", "RGB24"),
                      testColorConversion("YUV420P", "BGR24"),
                      testColorConversion("YUV420P", "GRAY8"),
                      testColorConversion("YUV420P", "YUV444P", 3),
                      testColorConversion("YUV420P", "YUV422P"),
                      testColorConversion("YUV420P", "YUV420P"),
                      testColorConversion("YUV420P", "YUV420SP_NV12"),
                      testColorConversion("YUV420P", "YUV420SP_NV21"),
                      testColorConversion("YUV420P", "HSV", 0.01),
                      testColorConversion("YUV420P", "Lab", 0.01),

                      // From NV12
                      testColorConversion("YUV420SP_NV12", "RGBA32"),
                      testColorConversion("YUV420SP_NV12", "BGRA32"),
                      testColorConversion("YUV420SP_NV12", "RGB24"),
                      testColorConversion("YUV420SP_NV12", "BGR24"),
                      testColorConversion("YUV420SP_NV12", "GRAY8"),
                      testColorConversion("YUV420SP_NV12", "YUV444P", 3),
                      testColorConversion("YUV420SP_NV12", "YUV422P"),
                      testColorConversion("YUV420SP_NV12", "YUV420P"),
                      testColorConversion("YUV420SP_NV12", "YUV420SP_NV12"),
                      testColorConversion("YUV420SP_NV12", "YUV420SP_NV21"),
                      testColorConversion("YUV420SP_NV12", "HSV", 0.01),
                      testColorConversion("YUV420SP_NV12", "Lab", 0.01),

                      // From NV21
                      testColorConversion("YUV420SP_NV21", "RGBA32"),
                      testColorConversion("YUV420SP_NV21", "BGRA32"),
                      testColorConversion("YUV420SP_NV21", "RGB24"),
                      testColorConversion("YUV420SP_NV21", "BGR24"),
                      testColorConversion("YUV420SP_NV21", "GRAY8"),
                      testColorConversion("YUV420SP_NV21", "YUV444P", 3),
                      testColorConversion("YUV420SP_NV21", "YUV422P"),
                      testColorConversion("YUV420SP_NV21", "YUV420P"),
                      testColorConversion("YUV420SP_NV21", "YUV420SP_NV12"),
                      testColorConversion("YUV420SP_NV21", "YUV420SP_NV21"),
                      testColorConversion("YUV420SP_NV21", "HSV", 0.01),
                      testColorConversion("YUV420SP_NV21", "Lab", 0.01),

                      // From HSV
                      testColorConversion("HSV", "RGBA32"),
                      testColorConversion("HSV", "BGRA32"),
                      testColorConversion("HSV", "RGB24"),
                      testColorConversion("HSV", "BGR24"),
                      testColorConversion("HSV", "GRAY8"),
                      testColorConversion("HSV", "YUV444P"),
                      testColorConversion("HSV", "YUV422P"),
                      testColorConversion("HSV", "YUV420P"),
                      testColorConversion("HSV", "YUV420SP_NV12"),
                      testColorConversion("HSV", "YUV420SP_NV21"),
                      testColorConversion("HSV", "HSV", 0),
                      testColorConversion("HSV", "Lab", 0.5),

                      // From Lab
                      testColorConversion("Lab", "RGBA32", 1),
                      testColorConversion("Lab", "BGRA32", 1),
                      testColorConversion("Lab", "RGB24", 1),
                      testColorConversion("Lab", "BGR24", 1),
                      testColorConversion("Lab", "GRAY8", 1),
                      testColorConversion("Lab", "YUV444P", 1),
                      testColorConversion("Lab", "YUV422P", 1),
                      testColorConversion("Lab", "YUV420P", 1),
                      testColorConversion("Lab", "YUV420SP_NV12", 1),
                      testColorConversion("Lab", "YUV420SP_NV21", 1),
                      testColorConversion("Lab", "HSV", 0.5),
                      testColorConversion("Lab", "Lab", 0),

                      // From GRAY8
                      testColorConversion("GRAY8", "GRAY8"),

                      // From DEPTH
                      testColorConversion("DEPTH", "DEPTH", 0, Uint16Array),
                     ]);
}

function testDraw() {
  return Promise.all([doOneDrawTest("RGB24"),
                      doOneDrawTest("BGR24"),
                      doOneDrawTest("YUV444P", 5),
                      doOneDrawTest("YUV422P", 2),
                      doOneDrawTest("YUV420P", 2),
                      doOneDrawTest("YUV420SP_NV12", 2),
                      doOneDrawTest("YUV420SP_NV21", 2),
                      doOneDrawTest("HSV", 2),
                      doOneDrawTest("Lab", 2)]);
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

function testColorConversion(sourceFromat, destinationFormat, tolerance, shouldThrow) {

  tolerance = tolerance || 0;
  shouldThrow = shouldThrow || false;

  return new Promise(function(resolve, reject) {
    var [srcData, dstData] = getTestData(sourceFromat, destinationFormat);

    ok(!!srcData, "Get valid srcData of type:" + sourceFromat);
    ok(!!dstData, "Get valid dstData of type:" + destinationFormat);

    // printInfo(sourceFromat, srcData);
    // printInfo(destinationFormat, dstData);

    // Create a new ImageBitmap from an ArrayBuffer.
    var p = createImageBitmap(srcData.buffer,
                              0,
                              srcData.bufferLength,
                              srcData.format,
                              srcData.pixelLayout);

    p.then(
      function(srcBitmap) {
        ok(!!srcBitmap, "Should get a valid srcBitmap.");
        ok(srcBitmap.findOptimalFormat() == sourceFromat, "srcBitmap.findOptimalFormat():" + srcBitmap.findOptimalFormat() +
                                                          " should equal to sourceFromat:" + sourceFromat);

        var dstBufferLength = srcBitmap.mappedDataLength(destinationFormat);
        var dstBuffer = new ArrayBuffer(dstBufferLength);
        var dstBufferView = new dstData.ArrayType(dstBuffer, 0, dstBufferLength / dstData.ArrayType.BYTES_PER_ELEMENT);

        // Do color conversion here.
        var p2 = srcBitmap.mapDataInto(destinationFormat, dstBuffer, 0);
        p2.then(
          function(dstPixelLayout) {
            var dataPixalLayout = dstData.pixelLayout;

            // Check pixel layout.
            ok(dstPixelLayout.length == dstData.channelCount, "dstPixelLayout.length:" + dstPixelLayout.length +
                                                              " should equal to dstData.channelCount:" + dstData.channelCount);

            for (var c = 0; c < dstData.channelCount; ++c) {
              var dstChannelLayout = dstPixelLayout[c];
              var dataChannelLayout = dataPixalLayout[c];
              ok(dstChannelLayout.width == dataChannelLayout.width, "channel[" + c + "] dstChannelLayout.width:" + dstChannelLayout.width + " should equal to dataChannelLayout.width:" + dataChannelLayout.width);
              ok(dstChannelLayout.height == dataChannelLayout.height, "channel[" + c + "] dstChannelLayout.height:" + dstChannelLayout.height + " should equal to dataChannelLayout.height:" + dataChannelLayout.height);
              ok(dstChannelLayout.skip == dataChannelLayout.skip, "channel[" + c + "] dstChannelLayout.skip:" + dstChannelLayout.skip + " should equal to dataChannelLayout.skip:" + dataChannelLayout.skip);

              for (var i = 0; i < dstChannelLayout.height; ++i) {
                for (var j = 0; j < dstChannelLayout.width; ++j) {
                  var byteOffset = dstChannelLayout.offset + i * dstChannelLayout.stride + j * (dstChannelLayout.skip + 1) * dstData.ArrayType.BYTES_PER_ELEMENT;
                  var view = new dstData.ArrayType(dstBuffer, byteOffset, 1);
                  var dstBufferViewValue = view[0];
                  var dstDataValue = dstData.getPixelValue(i, j, c);
                  ok(Math.abs(dstBufferViewValue - dstDataValue) <= tolerance,
                     "[" + sourceFromat + " -> " + destinationFormat + "] pixel(" + i + "," + j + ") channnel(" + c +
                     "): dstBufferViewValue:" + dstBufferViewValue +
                     " should equal to dstDataValue:" + dstDataValue);
                }
              }
            }

            resolve();
          },
          function(ev) {
            // If the "mapDataInto" throws, the flow goes here.
            if (!shouldThrow) { failed(ev); }
            reject();
          }
        );
      },
      function(ev) {
        reject(ev);
      }
    );
  });
}

function doOneDrawTest(sourceFromat, tolerance) {
  tolerance = tolerance || 0;
  var destinationFormat = "RGBA32";

  return new Promise(function(resolve, reject) {

    var [srcData, dstData] = getTestData(sourceFromat, destinationFormat);
    ok(!!srcData, "Get valid srcData of type:" + sourceFromat);
    ok(!!dstData, "Get valid dstData of type:" + destinationFormat);

    var p = createImageBitmap(srcData.buffer,
                              0,
                              srcData.bufferLength,
                              srcData.format,
                              srcData.pixelLayout);

    p.then(
      function(srcBitmap) {
        ok(!!srcBitmap, "Should get a valid srcBitmap.");
        ok(srcBitmap.findOptimalFormat() == sourceFromat, "srcBitmap.findOptimalFormat():" + srcBitmap.findOptimalFormat() +
                                                          " should equal to sourceFromat:" + sourceFromat);

        var canvas = document.createElement("canvas");
        canvas.width = srcBitmap.width;
        canvas.height = srcBitmap.height;
        var ctx = canvas.getContext("2d");

        ctx.drawImage(srcBitmap, 0, 0, srcBitmap.width, srcBitmap.height);

        // Get an ImageData from the canvas.
        var imageData = ctx.getImageData(0, 0, srcBitmap.width, srcBitmap.height);

        for (var i = 0; i < srcBitmap.height; ++i) {
          for (var j = 0; j < srcBitmap.width; ++j) {
            var pixelOffset = i * srcBitmap.width * dstData.channelCount + j * dstData.channelCount;
            var dstImageDataValue_R = imageData.data[pixelOffset + 0];
            var dstImageDataValue_G = imageData.data[pixelOffset + 1];
            var dstImageDataValue_B = imageData.data[pixelOffset + 2];
            var dstImageDataValue_A = imageData.data[pixelOffset + 3];

            var logPrefix = "[" + sourceFromat + " -> " + destinationFormat + "] pixel(" + i + "," + j + ")";

            var dstDataValue_R = dstData.getPixelValue(i, j, 0);
            var dstDataValue_G = dstData.getPixelValue(i, j, 1);
            var dstDataValue_B = dstData.getPixelValue(i, j, 2);
            var dstDataValue_A = dstData.getPixelValue(i, j, 3);
            ok(Math.abs(dstImageDataValue_R - dstDataValue_R) <= tolerance,
               logPrefix + "channnel(R): dstImageDataValue:" + dstImageDataValue_R + " should equal to dstDataValue_R: " + dstDataValue_R);
            ok(Math.abs(dstImageDataValue_G - dstDataValue_G) <= tolerance,
               logPrefix + "channnel(G): dstImageDataValue:" + dstImageDataValue_G + " should equal to dstDataValue_G: " + dstDataValue_G);
            ok(Math.abs(dstImageDataValue_B - dstDataValue_B) <= tolerance,
               logPrefix + "channnel(B): dstImageDataValue:" + dstImageDataValue_B + " should equal to dstDataValue_B: " + dstDataValue_B);
            ok(Math.abs(dstImageDataValue_A - dstDataValue_A) <= tolerance,
               logPrefix + "channnel(A): dstImageDataValue:" + dstImageDataValue_A + " should equal to dstDataValue_A: " + dstDataValue_A);
          }
        }

        resolve();
      },
      function(ev) {
        failed(ev);
        reject(ev);
      }
    );
  });
}