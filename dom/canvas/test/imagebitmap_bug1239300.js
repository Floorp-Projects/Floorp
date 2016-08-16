function testBug1239300() {
  return new Promise(function(resolve, reject) {
    createImageBitmap(new Blob()).then(
      function() {
        ok(false, "The promise should be rejected with InvalidStateError.");
        reject();
      },
      function(result) {
        if (result.name == "InvalidStateError") {
          ok(true, "The promise should be rejected with InvalidStateError.");
          resolve();
        } else {
          ok(false, "The promise should be rejected with InvalidStateError.");
          reject();
        }
      }
    );
  });
}