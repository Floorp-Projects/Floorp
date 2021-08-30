function waitForState(worker, state, context) {
  return new Promise(resolve => {
    function onStateChange() {
      if (worker.state === state) {
        worker.removeEventListener("statechange", onStateChange);
        resolve(context);
      }
    }

    // First add an event listener, so we won't miss any change that happens
    // before we check the current state.
    worker.addEventListener("statechange", onStateChange);

    // Now check if the worker is already in the desired state.
    onStateChange();
  });
}

/**
 * Helper for browser tests to issue register calls from the content global and
 * wait for the SW to progress to the active state, as most tests desire.
 * From the ContentTask.spawn, use via
 * `content.wrappedJSObject.registerAndWaitForActive`.
 */
async function registerAndWaitForActive(script, maybeScope) {
  console.log("...calling register");
  let opts = undefined;
  if (maybeScope) {
    opts = { scope: maybeScope };
  }
  const reg = await navigator.serviceWorker.register(script, opts);
  // Unless registration resurrection happens, the SW should be in the
  // installing slot.
  console.log("...waiting for activation");
  await waitForState(reg.installing, "activated", reg);
  console.log("...activated!");
  return reg;
}

/**
 * Helper to create an iframe with the given URL and return the first
 * postMessage payload received.  This is intended to be used when creating
 * cross-origin iframes.
 *
 * A promise will be returned that resolves with the payload of the postMessage
 * call.
 */
function createIframeAndWaitForMessage(url) {
  const iframe = document.createElement("iframe");
  document.body.appendChild(iframe);
  return new Promise(resolve => {
    window.addEventListener(
      "message",
      event => {
        resolve(event.data);
      },
      { once: true }
    );
    iframe.src = url;
  });
}

/**
 * Helper to create a nested iframe into the iframe created by
 * createIframeAndWaitForMessage().
 *
 * A promise will be returned that resolves with the payload of the postMessage
 * call.
 */
function createNestedIframeAndWaitForMessage(url) {
  const iframe = document.getElementsByTagName("iframe")[0];
  iframe.contentWindow.postMessage("create nested iframe", "*");
  return new Promise(resolve => {
    window.addEventListener(
      "message",
      event => {
        resolve(event.data);
      },
      { once: true }
    );
  });
}

async function unregisterAll() {
  const registrations = await navigator.serviceWorker.getRegistrations();
  for (const reg of registrations) {
    await reg.unregister();
  }
}

/**
 * Make a blob that contains random data and therefore shouldn't compress all
 * that well.
 */
function makeRandomBlob(size) {
  const arr = new Uint8Array(size);
  let offset = 0;
  /**
   * getRandomValues will only provide a maximum of 64k of data at a time and
   * will error if we ask for more, so using a while loop for get a random value
   * which much larger than 64k.
   * https://developer.mozilla.org/en-US/docs/Web/API/Crypto/getRandomValues#exceptions
   */
  while (offset < size) {
    const nextSize = Math.min(size - offset, 65536);
    window.crypto.getRandomValues(new Uint8Array(arr.buffer, offset, nextSize));
    offset += nextSize;
  }
  return new Blob([arr], { type: "application/octet-stream" });
}

async function fillStorage(cacheBytes, idbBytes) {
  // ## Fill Cache API Storage
  const cache = await caches.open("filler");
  await cache.put("fill", new Response(makeRandomBlob(cacheBytes)));

  // ## Fill IDB
  const storeName = "filler";
  let db = await new Promise((resolve, reject) => {
    let openReq = indexedDB.open("filler", 1);
    openReq.onerror = event => {
      reject(event.target.error);
    };
    openReq.onsuccess = event => {
      resolve(event.target.result);
    };
    openReq.onupgradeneeded = event => {
      const useDB = event.target.result;
      useDB.onerror = error => {
        reject(error);
      };
      const store = useDB.createObjectStore(storeName);
      store.put({ blob: makeRandomBlob(idbBytes) }, "filler-blob");
    };
  });
}
