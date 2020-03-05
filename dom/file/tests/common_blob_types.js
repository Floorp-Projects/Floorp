let blobTypes = [
  {
    type: "memory",
    factory: async content => {
      return new Blob([content]);
    },
    blobImplType: "MultipartBlobImpl[StringBlobImpl]",
  },

  {
    type: "ipcBlob",
    factory: async content => {
      return new Promise(resolve => {
        let bc1 = new BroadcastChannel("blob tests");
        bc1.onmessage = e => {
          resolve(e.data);
        };

        let bc2 = new BroadcastChannel("blob tests");
        bc2.postMessage(new Blob([content]));
      });
    },
    blobImplType:
      "StreamBlobImpl[StreamBlobImpl[MultipartBlobImpl[StringBlobImpl]]]",
  },

  {
    type: "memoryBlob",
    factory: async content => {
      return new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open(
          "POST",
          "http://mochi.test:8888/browser/dom/xhr/tests/temporaryFileBlob.sjs"
        );
        xhr.responseType = "blob";
        xhr.send(content);
        xhr.onloadend = _ => {
          resolve(xhr.response);
        };
      });
    },
    blobImplType: "MemoryBlobImpl",
  },

  {
    type: "temporaryBlob",
    factory: async content => {
      await SpecialPowers.pushPrefEnv({
        set: [["dom.blob.memoryToTemporaryFile", 1]],
      });

      return new Promise(resolve => {
        var xhr = new XMLHttpRequest();
        xhr.open(
          "POST",
          "http://mochi.test:8888/browser/dom/xhr/tests/temporaryFileBlob.sjs"
        );
        xhr.responseType = "blob";
        xhr.send(content);
        xhr.onloadend = _ => {
          resolve(xhr.response);
        };
      });
    },
    blobImplType: "StreamBlobImpl[TemporaryFileBlobImpl]",
  },
];

async function forEachBlobType(content, cb) {
  for (let i = 0; i < blobTypes.length; ++i) {
    info("Running tests for " + blobTypes[i].type);
    let blob = await blobTypes[i].factory(content);
    is(
      SpecialPowers.wrap(blob).blobImplType,
      blobTypes[i].blobImplType,
      "Correct blobImplType"
    );
    ok(blob instanceof Blob, "Blob created");
    await cb(blob, content);
  }
}
