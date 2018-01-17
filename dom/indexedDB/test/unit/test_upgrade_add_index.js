/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function generateKey() {
  let algorithm = {
    name: "RSASSA-PKCS1-v1_5",
    hash: "SHA-256",
    modulusLength: 1024,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01])
  };

  return crypto.subtle.generateKey(algorithm, true, ["sign", "verify"]);
}

const hasCrypto = "crypto" in this;

/**
 * Test addition of a new index when the existing values in the referenced
 * object store have potentially unusual structured clone participants.
 *
 * When a new index is created, the existing object store's contents need to be
 * processed to derive the index values.  This is a special event because
 * normally index values are extracted at add()/put() time in the content
 * process using the page/worker's JS context (modulo some spec stuff).  But
 * the index creation operation is actually running in the parent process on the
 * I/O thread for the given database.  So the JS context scenario is suddenly
 * a lot more complicated, and we need extra test coverage, in particular for
 * unusual structured clone payloads.
 *
 * Relationship to other test:
 * - test_create_index_with_integer_keys.js: This test is derived from that one.
 */
function* testSteps()
{
  // -- Create our fancy data that has interesting structured clone issues.
  const allData = [];

  // the xpcshell tests normalize self into existence.
  if (hasCrypto) {
    info("creating crypto key");
    // (all IDB tests badly need a test driver update...)
    generateKey().then(grabEventAndContinueHandler);
    let key = yield undefined;
    allData.push({
      id: 1,
      what: "crypto",
      data: key
    });
  } else {
    info("not storing crypto key");
  }

  if (isWasmSupported()) {
    info("creating wasm");
    getWasmBinary("(module (func (nop)))");
    let binary = yield undefined;
    allData.push({
      id: 2,
      what: "wasm",
      data: getWasmModule(binary)
    });
  } else {
    info("not storing wasm");
  }

  // -- Create the IDB and populate it with the base data.
  info("opening initial database");
  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler; // advance onupgradeneeded
  let event = yield undefined; // wait for onupgradeneeded.

  let db = event.target.result;
  db.onerror = errorHandler;

  event.target.onsuccess = continueToNextStep; // advance when the open completes

  // Make object store, add data.
  let objectStore = db.createObjectStore("foo", { keyPath: "id" });
  for (let datum of allData) {
    info(`add()ing ${datum.what}`);
    objectStore.add(datum);
  }
  // wait for the open to complete following our upgrade transaction self-closing
  yield undefined;
  // explicitly close the database so we can open it again.  We don't wait for
  // this, but the upgrade open will block until the close actually happens.
  info("closing initial database");
  db.close();

  // -- Trigger an upgrade, adding a new index.
  info("opening database for upgrade to v2");
  request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 2);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler; // advance onupgradeneeded
  event = yield undefined; // wait for onupgradeneeded

  let db2 = event.target.result;
  db2.onerror = errorHandler;

  event.target.onsuccess = continueToNextStep; // advance when the open completes

  // Create index.
  info("in upgrade, creating index");
  event.target.transaction.objectStore("foo").createIndex("foo", "what");
  yield undefined; // wait for the open to complete
  info("upgrade completed");

  finishTest();
}
