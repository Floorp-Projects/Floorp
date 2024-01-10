/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const isInChaosMode = () => {
  return !!parseInt(Services.env.get("MOZ_CHAOSMODE"), 16);
};

// Reduce the amount of data on slow platforms.
const getDataBlockSize = () => {
  if (mozinfo.os == "android") {
    // Android is much slower than desktop.
    if (mozinfo.verify) {
      return 54; // Chaos mode on android
    }

    return 3333;
  }

  if (isInChaosMode()) {
    return 333;
  }

  return 33333;
};

/* exported testSteps */
async function testSteps() {
  const name = this.window ? window.location.pathname : "Splendid Test";
  const request = indexedDB.open(name, 1);

  (ev => {
    const os = ev.target.result.createObjectStore("testObjectStore", {
      keyPath: "id",
      autoIncrement: false,
    });

    os.createIndex("testObjectStoreIndexA", "indexA", { unique: true });
    os.createIndex("testObjectStoreIndexB", "indexB", { unique: false });
    os.createIndex("testObjectStoreIndexC", "indexC", { unique: false });
  })(await expectingUpgrade(request));

  const db = (await expectingSuccess(request)).target.result;

  const objectStore = db
    .transaction(["testObjectStore"], "readwrite")
    .objectStore("testObjectStore");

  const dataBlock = getDataBlockSize();
  const lastIndex = 3 * dataBlock;

  info("We will now add " + lastIndex + " blobs to our object store");

  const addSegment = async (from, dataValue) => {
    for (let i = from; i <= from + dataBlock - 1; ++i) {
      await objectStore.add({
        id: i,
        indexA: i,
        indexB: lastIndex + 1 - i,
        indexC: i % 3,
        value: dataValue,
      });
    }
  };

  const expectedBegin = getRandomView(512);
  const expectedMiddle = getRandomView(512);
  const expectedEnd = getRandomView(512);
  ok(
    !compareBuffers(expectedBegin, expectedMiddle),
    "Are all buffers different?"
  );
  ok(!compareBuffers(expectedBegin, expectedEnd), "Are all buffers different?");
  ok(
    !compareBuffers(expectedMiddle, expectedEnd),
    "Are all buffers different?"
  );

  const dataValueBegin = getBlob(expectedBegin);
  await addSegment(1, dataValueBegin);

  const dataValueMiddle = getBlob(expectedMiddle);
  await addSegment(dataBlock + 1, dataValueMiddle);

  const dataValueEnd = getBlob(expectedEnd);
  await addSegment(2 * dataBlock + 1, dataValueEnd);

  // Performance issue of 1860486 occurs here
  await new Promise((res, rej) => {
    let isDone = false;
    const deleteReq = objectStore.delete(
      IDBKeyRange.bound(6, lastIndex - 5, false, false)
    );
    deleteReq.onsuccess = () => {
      isDone = true;
      res();
    };
    deleteReq.onerror = err => {
      isDone = true;
      rej(err);
    };

    /**
     * The deletion should be over in 20 seconds or less on desktop. With the
     * regression, the operation can take more than 30 minutes. We use one
     * minute to reduce intermittent failures due to the CI environment.
     *
     * Note that this is not a magical timeout for the completion of an
     * asynchronous request: we are testing a hang and using an explicit timeout
     * will avoid the much longer default timeout which is way too long to be
     * acceptable in real use cases.
     *
     * Maintenance plan: If disk contention and slow hardware lead to too many
     * intermittent failures, the regression cutoff could be increased to 2-3
     * minutes or the test could be turned into a raptor performance test.
     */
    const minutes = 60 * 1000;
    const performance_regression_cutoff = 1 * minutes;
    do_timeout(performance_regression_cutoff, () => {
      if (!isDone) {
        rej(Error("Performance regression detected"));
      }
    });
  });

  const getIndexedItems = async indexName => {
    let actuals = [];
    return new Promise(res => {
      db
        .transaction(["testObjectStore"], "readonly")
        .objectStore("testObjectStore")
        .index(indexName)
        .openCursor().onsuccess = ev => {
        const cursor = ev.target.result;
        if (!cursor) {
          res(actuals);
        } else {
          actuals.push(cursor.value.value);
          cursor.continue();
        }
      };
    });
  };

  const checkValuesEqualTo = async (actuals, from, to, expected) => {
    for (let i = from; i < to; ++i) {
      const actual = new Uint8Array(await actuals[i].arrayBuffer());
      if (!compareBuffers(actual, expected)) {
        return i;
      }
    }
    return undefined;
  };

  const itemsA = await getIndexedItems("testObjectStoreIndexA");
  equal(itemsA.length, 10);

  const mismatchABegin = await checkValuesEqualTo(itemsA, 0, 5, expectedBegin);
  equal(
    mismatchABegin,
    undefined,
    "First index with value mismatch is " + mismatchABegin
  );
  const mismatchAEnd = await checkValuesEqualTo(itemsA, 5, 10, expectedEnd);
  equal(
    mismatchAEnd,
    undefined,
    "First index with value mismatch is " + mismatchAEnd
  );

  const itemsB = await getIndexedItems("testObjectStoreIndexB");

  equal(itemsB.length, 10);
  const mismatchBEnd = await checkValuesEqualTo(itemsB, 0, 5, expectedEnd);
  equal(
    mismatchBEnd,
    undefined,
    "First index with value mismatch is " + mismatchBEnd
  );
  const mismatchBBegin = await checkValuesEqualTo(itemsB, 5, 10, expectedBegin);
  equal(
    mismatchBBegin,
    undefined,
    "First index with value mismatch is " + mismatchBBegin
  );

  const actualsC = await getIndexedItems("testObjectStoreIndexC");

  equal(actualsC.length, 10);
  let countBegin = 0;
  let countEnd = 0;
  for (let i = 0; i < 10; ++i) {
    const actual = new Uint8Array(await actualsC[i].arrayBuffer());
    if (compareBuffers(actual, expectedBegin)) {
      ++countBegin;
    } else if (compareBuffers(actual, expectedEnd)) {
      ++countEnd;
    }
  }

  equal(countBegin, 5);
  equal(countEnd, 5);

  await db.close();
}
