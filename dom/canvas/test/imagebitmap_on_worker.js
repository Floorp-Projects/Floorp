importScripts("imagebitmap_bug1239300.js");

function ok(expect, msg) {
  postMessage({ type: "status", status: !!expect, msg });
}

function doneTask() {
  postMessage({ type: "doneTask" });
}

function promiseThrows(p, name) {
  var didThrow;
  return p
    .then(
      function() {
        didThrow = false;
      },
      function() {
        didThrow = true;
      }
    )
    .then(function() {
      ok(didThrow, "[TestException] " + name);
    });
}

onmessage = function(event) {
  if (event.data.type == "testImageData") {
    var width = event.data.width;
    var height = event.data.height;
    var imageData = event.data.source;
    ok(imageData, "[CreateFromImageData] An ImageData is passed into worker.");
    ok(
      imageData.width == width,
      "[CreateFromImageData] Passed ImageData has right width = " + width
    );
    ok(
      imageData.height == height,
      "[CreateFromImageData] Passed ImageData has right height = " + height
    );

    var promise = createImageBitmap(imageData);
    promise.then(function(bitmap) {
      ok(bitmap, "[CreateFromImageData] ImageBitmap is created successfully.");
      ok(
        bitmap.width == width,
        "[CreateFromImageData] ImageBitmap.width = " +
          bitmap.width +
          ", expected witdth = " +
          width
      );
      ok(
        bitmap.height == height,
        "[CreateFromImageData] ImageBitmap.height = " +
          bitmap.height +
          ", expected height = " +
          height
      );

      doneTask();
    });
  } else if (event.data.type == "testBlob") {
    var width = event.data.width;
    var height = event.data.height;
    var blob = event.data.source;
    ok(blob, "[CreateFromBlob] A Blob object is passed into worker.");

    var promise = createImageBitmap(blob);
    promise.then(function(bitmap) {
      ok(bitmap, "[CreateFromBlob] ImageBitmap is created successfully.");
      ok(
        bitmap.width == width,
        "[CreateFromBlob] ImageBitmap.width = " +
          bitmap.width +
          ", expected witdth = " +
          width
      );
      ok(
        bitmap.height == height,
        "[CreateFromBlob] ImageBitmap.height = " +
          bitmap.height +
          ", expected height = " +
          height
      );

      doneTask();
    });
  } else if (event.data.type == "testImageBitmap") {
    var width = event.data.width;
    var height = event.data.height;
    var source = event.data.source;
    ok(
      source,
      "[CreateFromImageBitmap] A soruce object is passed into worker."
    );

    var promise = createImageBitmap(source);
    promise.then(function(bitmap) {
      ok(
        bitmap,
        "[CreateFromImageBitmap] ImageBitmap is created successfully."
      );
      ok(
        bitmap.width == width,
        "[CreateFromImageBitmap] ImageBitmap.width = " +
          bitmap.width +
          ", expected witdth = " +
          width
      );
      ok(
        bitmap.height == height,
        "[CreateFromImageBitmap] ImageBitmap.height = " +
          bitmap.height +
          ", expected height = " +
          height
      );

      var promise2 = createImageBitmap(bitmap);
      promise2.then(function(bitmap2) {
        ok(
          bitmap2,
          "[CreateFromImageBitmap] 2nd ImageBitmap is created successfully."
        );
        ok(
          bitmap.width == width,
          "[CreateFromImageBitmap] ImageBitmap.width = " +
            bitmap.width +
            ", expected witdth = " +
            width
        );
        ok(
          bitmap.height == height,
          "[CreateFromImageBitmap] ImageBitmap.height = " +
            bitmap.height +
            ", expected height = " +
            height
        );

        doneTask();
      });
    });
  } else if (event.data.type == "testException") {
    var source = event.data.source;
    if (event.data.sx) {
      var sx = event.data.sx;
      var sy = event.data.sy;
      var sw = event.data.sw;
      var sh = event.data.sh;
      promiseThrows(createImageBitmap(source, sx, sy, sw, sh), event.data.msg);
    } else {
      promiseThrows(createImageBitmap(source), event.data.msg);
    }
    doneTask();
  } else if (event.data.type == "testBug1239300") {
    var promise = testBug1239300();
    promise.then(doneTask, doneTask);
  }
};
