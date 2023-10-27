const SAME_COMPARTMENT = "same-compartment";
const IFRAME_COMPARTMENT = "iframe-compartment";
const BIG_BUFFER_SIZE = 1000000;
const ITER_MAX = 10;

function makeBuffer(size) {
  let buffer = new Uint8Array(size);
  buffer.fill(42);

  let value = 0;
  for (let i = 0; i < 1000000; i += 1000) {
    buffer.set([++value % 255], i);
  }

  return buffer;
}

function apply_compartment(compartment, data) {
  if (compartment == SAME_COMPARTMENT) {
    return self[data.func](data.args, self);
  }

  if (compartment == IFRAME_COMPARTMENT) {
    const iframe = document.querySelector("#iframe").contentWindow;
    return iframe[data.func](data.args, self);
  }

  ok(false, "Invalid compartment value");
}

async function test_nativeStream(compartment) {
  info("test_nativeStream");

  let r = await fetch("/");

  return apply_compartment(compartment, {
    func: "test_nativeStream_continue",
    args: r,
  });
}

async function test_nativeStream_continue(r, that) {
  that.ok(r.body instanceof that.ReadableStream, "We have a ReadableStream");

  let a = r.clone();
  that.ok(a instanceof that.Response, "We have a cloned Response");
  that.ok(a.body instanceof that.ReadableStream, "We have a ReadableStream");

  let b = a.clone();
  that.ok(b instanceof that.Response, "We have a cloned Response");
  that.ok(b.body instanceof that.ReadableStream, "We have a ReadableStream");

  let blob = await r.blob();

  that.ok(blob instanceof that.Blob, "We have a blob");
  let d = await a.body.getReader().read();

  that.ok(!d.done, "We have read something!");
  blob = await b.blob();

  that.ok(blob instanceof that.Blob, "We have a blob");
}

async function test_timeout(compartment) {
  info("test_timeout");

  let blob = new Blob([""]);
  let r = await fetch(URL.createObjectURL(blob));

  return apply_compartment(compartment, {
    func: "test_timeout_continue",
    args: r,
  });
}

async function test_timeout_continue(r, that) {
  await r.body.getReader().read();

  await new Promise(resolve => setTimeout(resolve, 0));

  try {
    await r.blob();
    that.ok(false, "We cannot have a blob here!");
  } catch (exc) {
    that.ok(true, "We cannot have a blob here!");
  }
}

async function test_nonNativeStream(compartment) {
  info("test_nonNativeStream");

  let buffer = makeBuffer(BIG_BUFFER_SIZE);
  info("Buffer size: " + buffer.byteLength);

  let r = new Response(
    new ReadableStream({
      start: controller => {
        controller.enqueue(buffer);
        controller.close();
      },
    })
  );

  return apply_compartment(compartment, {
    func: "test_nonNativeStream_continue",
    args: { r, buffer },
  });
}

async function test_nonNativeStream_continue(data, that) {
  that.ok(
    data.r.body instanceof that.ReadableStream,
    "We have a ReadableStream"
  );

  let a = data.r.clone();
  that.ok(a instanceof that.Response, "We have a cloned Response");
  that.ok(a.body instanceof that.ReadableStream, "We have a ReadableStream");

  let b = a.clone();
  that.ok(b instanceof that.Response, "We have a cloned Response");
  that.ok(b.body instanceof that.ReadableStream, "We have a ReadableStream");

  let blob = await data.r.blob();

  that.ok(blob instanceof that.Blob, "We have a blob");
  let d = await a.body.getReader().read();

  that.ok(!d.done, "We have read something!");
  blob = await b.blob();

  that.ok(blob instanceof that.Blob, "We have a blob");
  that.is(blob.size, data.buffer.byteLength, "Blob size matches");
}

async function test_noUint8Array(compartment) {
  info("test_noUint8Array");

  let r = new Response(
    new ReadableStream({
      start: controller => {
        controller.enqueue("hello world!");
        controller.close();
      },
    })
  );

  return apply_compartment(compartment, {
    func: "test_noUint8Array_continue",
    args: r,
  });
}

async function test_noUint8Array_continue(r, that) {
  that.ok(r.body instanceof that.ReadableStream, "We have a ReadableStream");

  try {
    await r.blob();
    that.ok(false, "We cannot have a blob here!");
  } catch (ex) {
    that.ok(true, "We cannot have a blob here!");
  }
}

async function test_pendingStream(compartment) {
  let r = new Response(
    new ReadableStream({
      start: controller => {
        controller.enqueue(makeBuffer(BIG_BUFFER_SIZE));
        // Let's keep this controler open.
        self.ccc = controller;
      },
    })
  );

  return apply_compartment(compartment, {
    func: "test_pendingStream_continue",
    args: r,
  });
}

async function test_pendingStream_continue(r, that) {
  let d = await r.body.getReader().read();

  that.ok(!d.done, "We have read something!");

  if ("close" in that) {
    that.close();
  }
}

async function test_nativeStream_cache(compartment) {
  info("test_nativeStream_cache");

  let origBody = "123456789abcdef";
  let url = "/nativeStream";

  let cache = await caches.open("nativeStream");

  info("Storing a body as a string");
  await cache.put(url, new Response(origBody));

  return apply_compartment(compartment, {
    func: "test_nativeStream_cache_continue",
    args: { caches, cache, url, origBody },
  });
}

async function test_nativeStream_cache_continue(data, that) {
  that.info("Retrieving the stored value");
  let cacheResponse = await data.cache.match(data.url);

  that.info("Converting the response to text");
  let cacheBody = await cacheResponse.text();

  that.is(data.origBody, cacheBody, "Bodies match");

  await data.caches.delete("nativeStream");
}

async function test_nonNativeStream_cache(compartment) {
  info("test_nonNativeStream_cache");

  let url = "/nonNativeStream";

  let cache = await caches.open("nonNativeStream");
  let buffer = makeBuffer(BIG_BUFFER_SIZE);
  info("Buffer size: " + buffer.byteLength);

  info("Storing a body as a string");
  let r = new Response(
    new ReadableStream({
      start: controller => {
        controller.enqueue(buffer);
        controller.close();
      },
    })
  );

  return apply_compartment(compartment, {
    func: "test_nonNativeStream_cache_continue",
    args: { caches, cache, buffer, r },
  });
}

async function test_nonNativeStream_cache_continue(data, that) {
  await data.cache.put(data.url, data.r);

  that.info("Retrieving the stored value");
  let cacheResponse = await data.cache.match(data.url);

  that.info("Converting the response to text");
  let cacheBody = await cacheResponse.arrayBuffer();

  that.ok(cacheBody instanceof that.ArrayBuffer, "Body is an array buffer");
  that.is(cacheBody.byteLength, BIG_BUFFER_SIZE, "Body length is correct");

  let value = 0;
  for (let i = 0; i < 1000000; i += 1000) {
    that.is(
      new Uint8Array(cacheBody)[i],
      ++value % 255,
      "byte in position " + i + " is correct"
    );
  }

  await data.caches.delete("nonNativeStream");
}

async function test_codeExecution(compartment) {
  info("test_codeExecution");

  let r = new Response(
    new ReadableStream({
      start(c) {
        controller = c;
      },
      pull() {
        console.log("pull called");
      },
    })
  );

  return apply_compartment(compartment, {
    func: "test_codeExecution_continue",
    args: r,
  });
}

// This is intended to just be a drop-in replacement for an old observer
// notification.
function addConsoleStorageListener(listener) {
  const ConsoleAPIStorage = SpecialPowers.Cc[
    "@mozilla.org/consoleAPI-storage;1"
  ].getService(SpecialPowers.Ci.nsIConsoleAPIStorage);
  listener.__handler = (message, id) => {
    listener.observe(message, id);
  };
  ConsoleAPIStorage.addLogEventListener(
    listener.__handler,
    SpecialPowers.wrap(document).nodePrincipal
  );
}

function removeConsoleStorageListener(listener) {
  const ConsoleAPIStorage = SpecialPowers.Cc[
    "@mozilla.org/consoleAPI-storage;1"
  ].getService(SpecialPowers.Ci.nsIConsoleAPIStorage);
  ConsoleAPIStorage.removeLogEventListener(listener.__handler);
}

async function test_codeExecution_continue(r, that) {
  function consoleListener() {
    addConsoleStorageListener(this);
  }

  var promise = new Promise(resolve => {
    consoleListener.prototype = {
      observe(aSubject) {
        that.ok(true, "Something has been received");

        var obj = aSubject.wrappedJSObject;
        if (obj.arguments[0] && obj.arguments[0] === "pull called") {
          that.ok(true, "Message received!");
          removeConsoleStorageListener(this);
          resolve();
        }
      },
    };
  });

  var cl = new consoleListener();

  r.body.getReader().read();
  await promise;
}

async function test_global(compartment) {
  info("test_global: " + compartment);

  self.foo = 42;
  self.iter = ITER_MAX;

  let r = new Response(
    new ReadableStream({
      start(c) {
        self.controller = c;
      },
      pull() {
        if (!("iter" in self) || self.iter < 0 || self.iter > ITER_MAX) {
          throw "Something bad is happening here!";
        }

        let buffer = new Uint8Array(1);
        buffer.fill(self.foo);
        self.controller.enqueue(buffer);

        if (--self.iter == 0) {
          controller.close();
        }
      },
    })
  );

  return apply_compartment(compartment, {
    func: "test_global_continue",
    args: r,
  });
}

async function test_global_continue(r, that) {
  let a = await r.arrayBuffer();

  that.is(
    Object.getPrototypeOf(a),
    that.ArrayBuffer.prototype,
    "Body is an array buffer"
  );
  that.is(a.byteLength, ITER_MAX, "Body length is correct");

  for (let i = 0; i < ITER_MAX; ++i) {
    that.is(new Uint8Array(a)[i], 42, "Byte " + i + " is correct");
  }
}

function workify(func) {
  info("Workifying " + func);

  return new Promise((resolve, reject) => {
    let worker = new Worker("worker_readableStreams.js");
    worker.postMessage(func);
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
