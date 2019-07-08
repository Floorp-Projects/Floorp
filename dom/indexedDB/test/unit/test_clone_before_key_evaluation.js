/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const name = this.window
    ? window.location.pathname
    : "test_clone_before_key_evaluation.js";
  const objectStoreInfo = {
    name: "customers",
    options: { keyPath: "ssn" },
  };
  const indexInfo = {
    name: "customerIndex",
    keyPath: ["id", "email", "name"],
    options: { unique: false },
  };

  for (let test of [1, 2]) {
    info("Opening database");

    let request = indexedDB.open(name);
    request.onerror = errorHandler;
    request.onupgradeneeded = continueToNextStepSync;
    request.onsuccess = unexpectedSuccessHandler;
    yield undefined;

    // upgradeneeded

    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = continueToNextStepSync;

    let db = request.result;
    db.onerror = errorHandler;

    info("Creating objectStore");

    let objectStore = db.createObjectStore(
      objectStoreInfo.name,
      objectStoreInfo.options
    );

    info("Creating index");

    objectStore.createIndex(
      indexInfo.name,
      indexInfo.keyPath,
      indexInfo.options
    );

    switch (test) {
      case 1: {
        info("Adding data with a getter");

        let idCount = 0;

        const customerData = {
          ssn: "444-44-4444",
          name: "Bill",
          age: 25,
          email: "bill@company.com",
          get id() {
            idCount++;
            objectStore.deleteIndex(indexInfo.name);
            return "ID_001";
          },
        };

        objectStore.add(customerData);

        ok(idCount == 1, "Getter was called only once");

        ok(objectStore.indexNames.length == 0, "Index was removed");

        break;
      }

      case 2: {
        info("Adding data with a prototype getter");

        let idCount = 0;

        const customerData = {
          ssn: "555-55-5555",
          name: "Joe",
          age: 52,
          email: "joe@company.com",
        };

        Object.defineProperty(Object.prototype, "id", {
          get() {
            idCount++;
            objectStore.deleteIndex(indexInfo.name);
            return "ID_002";
          },
          enumerable: false,
          configurable: true,
        });

        objectStore.add(customerData);

        ok(idCount == 0, "Prototype getter was not called");

        // Paranoid checks, just to be sure that the protype getter is called
        // in standard JS.

        let id = customerData.id;

        ok(id == "ID_002", "Prototype getter returned correct value");
        ok(idCount == 1, "Prototype getter was called only once");

        delete Object.prototype.id;

        id = customerData.id;

        ok(id == undefined, "Prototype getter was removed");

        ok(objectStore.indexNames.length == 0, "Index was removed");

        break;
      }
    }

    yield undefined;

    // success

    db.close();

    request = indexedDB.deleteDatabase(name);
    request.onerror = errorHandler;
    request.onsuccess = continueToNextStepSync;
    yield undefined;
  }

  finishTest();
}
