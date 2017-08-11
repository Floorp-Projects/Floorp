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
