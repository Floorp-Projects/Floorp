/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const Bob = { ss: "237-23-7732", name: "Bob" };

  let request = indexedDB.open(this.window ? window.location.pathname : "Splendid Test", 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  event.target.onsuccess = continueToNextStep;

  let objectStore = db.createObjectStore("foo", { keyPath: "ss" });
  objectStore.createIndex("name", "name", { unique: true });
  objectStore.add(Bob);
  yield undefined;

  // This function expression causes objectStore to be aliased, and thus
  // allocated on the scope chain.  Comment it out and the test passes.
  // Bug 943409.
  (function() { objectStore });

  db.transaction("foo", "readwrite").objectStore("foo")
    .index("name").openCursor().onsuccess = function(event) {
    event.target.transaction.oncomplete = continueToNextStep;
    let cursor = event.target.result;
    if (cursor) {
      let objectStore = event.target.transaction.objectStore("foo");
      objectStore.delete(Bob.ss)
                 .onsuccess = function(event) { cursor.continue(); };
    }
  };
  yield undefined;
  finishTest();

  // This workaround allows this cycle test to pass, as the data
  // associated with the transaction is collected, and thus there is no
  // more cycle in which the outer objectStore participates.  Bug
  // 943409.
  gc();

  yield undefined;
}
