function ok(expect, msg) {
  postMessage({"type": "status", status: !!expect, msg: msg});
}

onmessage = function(event) {
  ok(!!event.data.bitmap1, "Get the 1st ImageBitmap from the main script.");
  ok(!!event.data.bitmap2, "Get the 2nd ImageBitmap from the main script.");
  ok(!!event.data.bitmap3, "Get the 3rd ImageBitmap from the main script.");

  // send the first original ImageBitmap back to the main-thread
  postMessage({"type":"bitmap1",
               "bitmap":event.data.bitmap1});

  // create a new ImageBitmap from the 2nd original ImageBitmap
  // and then send the newly created ImageBitmap back to the main-thread
  var promise = createImageBitmap(event.data.bitmap2);
  promise.then(
    function(bitmap) {
      ok(true, "Successfully create a new ImageBitmap from the 2nd original bitmap in worker.");

      // send the newly created ImageBitmap back to the main-thread
      postMessage({"type":"bitmap2", "bitmap":bitmap});

      // finish the test
      postMessage({"type": "finish"});
    },
    function() {
      ok(false, "Cannot create a new bitmap from the original bitmap in worker.");
    }
  );

  // send the third original ImageBitmap back to the main-thread
  postMessage({"type":"bitmap3",
               "bitmap":event.data.bitmap3});
}
