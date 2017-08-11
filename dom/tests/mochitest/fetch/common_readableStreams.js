function test_nativeStream() {
  info("test_nativeStream");

  fetch('/').then(r => {
    ok(r.body instanceof ReadableStream, "We have a ReadableStream");

    let a = r.clone();
    ok(a.body instanceof ReadableStream, "We have a ReadableStream");

    let b = a.clone();
    ok(b.body instanceof ReadableStream, "We have a ReadableStream");

    r.blob().then(b => {
      ok(b instanceof Blob, "We have a blob");
      return a.body.getReader().read();
    }).then(d => {
      ok(!d.done, "We have read something!");
      return b.blob();
    }).then(b => {
      ok(b instanceof Blob, "We have a blob");
    }).then(next);
  });
}

function test_nonNativeStream() {
  info("test_nonNativeStream");

  let r = new Response(new ReadableStream({start : controller => {
    controller.enqueue(new Uint8Array([0x01, 0x00, 0x01]));
    controller.close();
  }}));

  ok(r.body instanceof ReadableStream, "We have a ReadableStream");

  let a = r.clone();
  ok(a.body instanceof ReadableStream, "We have a ReadableStream");

  let b = a.clone();
  ok(b.body instanceof ReadableStream, "We have a ReadableStream");

  r.blob().then(b => {
    ok(b instanceof Blob, "We have a blob");
    return a.body.getReader().read();
  }).then(d => {
    ok(!d.done, "We have read something!");
    return b.blob();
  }).then(b => {
    ok(b instanceof Blob, "We have a blob");
  }).then(next);
}

function test_noUint8Array() {
  info("test_noUint8Array");

  let r = new Response(new ReadableStream({start : controller => {
    controller.enqueue('hello world!');
    controller.close();
  }}));

  ok(r.body instanceof ReadableStream, "We have a ReadableStream");

  r.blob().then(b => {
    ok(false, "We cannot have a blob here!");
  }, () => {
    ok(true, "We cannot have a blob here!");
  }).then(next);
}

function test_pendingStream() {
  var r = new Response(new ReadableStream({start : controller => {
    controller.enqueue(new Uint8Array([0x01, 0x00, 0x01]));
    // Let's keep this controler open.
    self.ccc = controller;
  }}));

  r.body.getReader().read().then(d => {
    ok(!d.done, "We have read something!");
    close();
    next();
  });
}

async function test_nativeStream_cache() {
  info("test_nativeStream_cache");

  let origBody = '123456789abcdef';
  let url = '/nativeStream';

  let cache = await caches.open('nativeStream');

  info("Storing a body as a string");
  await cache.put(url, new Response(origBody));

  info("Retrieving the stored value");
  let cacheResponse = await cache.match(url);

  info("Converting the response to text");
  let cacheBody = await cacheResponse.text();

  is(origBody, cacheBody, "Bodies match");

  await caches.delete('nativeStream');

  next();
};

async function test_nonNativeStream_cache() {
  info("test_nonNativeStream_cache");

  let url = '/nonNativeStream';

  let cache = await caches.open('nonNativeStream');

  info("Storing a body as a string");
  let r = new Response(new ReadableStream({start : controller => {
    controller.enqueue(new Uint8Array([0x01, 0x02, 0x03]));
    controller.close();
  }}));

  await cache.put(url, r);

  info("Retrieving the stored value");
  let cacheResponse = await cache.match(url);

  info("Converting the response to text");
  let cacheBody = await cacheResponse.arrayBuffer();

  ok(cacheBody instanceof ArrayBuffer, "Body is an array buffer");
  is(cacheBody.byteLength, 3, "Body length is correct");

  is(new Uint8Array(cacheBody)[0], 0x01, "First byte is correct");
  is(new Uint8Array(cacheBody)[1], 0x02, "Second byte is correct");
  is(new Uint8Array(cacheBody)[2], 0x03, "Third byte is correct");

  await caches.delete('nonNativeStream');

  next();
};

function workify(func) {
  info("Workifing " + func);

  let worker = new Worker('worker_readableStreams.js');
  worker.postMessage(func);
  worker.onmessage = function(e) {
    if (e.data.type == 'done') {
      next();
      return;
    }

    if (e.data.type == 'test') {
      ok(e.data.test, e.data.message);
      return;
    }

    if (e.data.type == 'info') {
      info(e.data.message);
      return;
    }
  }

  return worker;
}
