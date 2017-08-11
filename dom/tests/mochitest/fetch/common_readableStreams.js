const BIG_BUFFER_SIZE = 1000000;

function makeBuffer(size) {
  let buffer = new Uint8Array(size);
  buffer.fill(42);

  let value = 0;
  for (let i = 0; i < 1000000; i+= 1000) {
    buffer.set([++value % 255], i);
  }

  return buffer;
}

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

  let buffer = makeBuffer(BIG_BUFFER_SIZE);
  info("Buffer size: " + buffer.byteLength);

  let r = new Response(new ReadableStream({start : controller => {
    controller.enqueue(buffer);
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
    is(b.size, buffer.byteLength, "Blob size matches");
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
    controller.enqueue(makeBuffer(BIG_BUFFER_SIZE));
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
  let buffer = makeBuffer(BIG_BUFFER_SIZE);
  info("Buffer size: " + buffer.byteLength);

  info("Storing a body as a string");
  let r = new Response(new ReadableStream({start : controller => {
    controller.enqueue(buffer);
    controller.close();
  }}));

  await cache.put(url, r);

  info("Retrieving the stored value");
  let cacheResponse = await cache.match(url);

  info("Converting the response to text");
  let cacheBody = await cacheResponse.arrayBuffer();

  ok(cacheBody instanceof ArrayBuffer, "Body is an array buffer");
  is(cacheBody.byteLength, BIG_BUFFER_SIZE, "Body length is correct");

  let value = 0;
  for (let i = 0; i < 1000000; i+= 1000) {
    is(new Uint8Array(cacheBody)[i], ++value % 255, "byte in position " + i + " is correct");
  }

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
