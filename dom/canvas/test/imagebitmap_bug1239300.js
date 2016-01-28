function testBug1239300() {
  return new Promise(function(resolve, reject) {
    createImageBitmap(new Blob()).then(
      function() {
        ok(false, "The promise should be rejected with null.");
        reject();
      },
      function(result) {
        if (result == null) {
          ok(true, "The promise should be rejected with null.");
          resolve();
        } else {
          ok(false, "The promise should be rejected with null.");
          reject();
        }
      }
    );
  });
}