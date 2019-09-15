add_task(async _ => {
  await new Promise(resolve => {
    var xhr = new XMLHttpRequest();
    xhr.open(
      "POST",
      "http://mochi.test:8888/browser/dom/xhr/tests/temporaryFileBlob.sjs"
    );
    xhr.responseType = "blob";
    xhr.send("");
    xhr.onloadend = __ => {
      is(xhr.response.blobImplType, "EmptyBlobImpl", "We want a EmptyBlobImpl");
      resolve();
    };
  });
});

add_task(async _ => {
  var data = new Array(2).join("1234567890ABCDEF");

  await new Promise(resolve => {
    var xhr = new XMLHttpRequest();
    xhr.open(
      "POST",
      "http://mochi.test:8888/browser/dom/xhr/tests/temporaryFileBlob.sjs"
    );
    xhr.responseType = "blob";
    xhr.send({
      toString() {
        return data;
      },
    });
    xhr.onloadend = __ => {
      is(
        xhr.response.blobImplType,
        "MemoryBlobImpl",
        "We want a MemoryBlobImpl"
      );
      resolve();
    };
  });
});

add_task(async _ => {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.blob.memoryToTemporaryFile", 1]],
  });

  var data = new Array(2).join("1234567890ABCDEF");

  await new Promise(resolve => {
    var xhr = new XMLHttpRequest();
    xhr.open(
      "POST",
      "http://mochi.test:8888/browser/dom/xhr/tests/temporaryFileBlob.sjs"
    );
    xhr.responseType = "blob";
    xhr.send({
      toString() {
        return data;
      },
    });
    xhr.onloadend = __ => {
      is(
        xhr.response.blobImplType,
        "StreamBlobImpl[TemporaryBlobImpl]",
        "We want a StreamBlobImpl holding a TemporaryBlobImpl on the parent side"
      );
      resolve();
    };
  });
});
