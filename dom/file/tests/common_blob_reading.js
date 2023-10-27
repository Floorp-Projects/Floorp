async function testBlobText(blob, content) {
  let text = await blob.text();
  is(text, content, "blob.text()");
}

async function testBlobArrayBuffer(blob, content) {
  let ab = await blob.arrayBuffer();
  is(ab.byteLength, content.length, "blob.arrayBuffer()");
}

async function testBlobStream(blob, content) {
  let s = await blob.stream();
  ok(s instanceof ReadableStream, "We have a ReadableStream");

  let data = await s.getReader().read();
  ok(!data.done, "Nothing is done yet");
  for (let i = 0; i < data.value.length; ++i) {
    is(String.fromCharCode(data.value[i]), content[i], "blob.stream() - " + i);
  }
}

function workify(func, blob, content) {
  info("Workifying " + func);

  return new Promise((resolve, reject) => {
    let worker = new Worker("worker_blob_reading.js");
    worker.postMessage({ func, blob, content });
    worker.onmessage = function (e) {
      if (e.data.type == "done") {
        resolve();
        return;
      }

      if (e.data.type == "error") {
        reject(e.data.message);
        return;
      }

      if (e.data.type == "test") {
        ok(e.data.test, e.data.message);
        return;
      }

      if (e.data.type == "info") {
        info(e.data.message);
      }
    };
  });
}
