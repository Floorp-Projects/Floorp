/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const interpretChar = (chars, index) => {
  return chars
    .charCodeAt(index)
    .toString(16)
    .padStart(4, "0");
};

const hexEncode = str => {
  let result = "";
  const len = str.length;
  for (let i = 0; i < len; ++i) {
    result += interpretChar(str, i);
  }
  return result;
};

const collectCorrupted = (expected, actual) => {
  const len = Math.min(expected.length, actual.length);
  let notEquals = [];
  for (let i = 0; i < len; ++i) {
    if (expected[i] !== actual[i]) {
      notEquals.push([hexEncode(expected[i]), hexEncode(actual[i])]);
    }
  }
  return notEquals;
};

const sanitizeOutputWithSurrogates = (testValue, prefix = "") => {
  let utf8What = prefix;
  for (let i = 0; i < testValue.length; ++i) {
    const valueChar = testValue.charCodeAt(i);
    const isPlanar = 0xd800 <= valueChar && valueChar <= 0xdfff;
    utf8What += isPlanar ? "\\u" + interpretChar(testValue, i) : testValue[i];
  }
  return utf8What;
};

const getEncodingSample = () => {
  const expectedSample =
    "3681207208613504e0a5028800b945551988c60050008027ebc2808c00d38e806e03d8210ac906722b85499be9d00000";

  let result = "";
  const len = expectedSample.length;
  for (let i = 0; i < len; i += 4) {
    result += String.fromCharCode(parseInt(expectedSample.slice(i, i + 4), 16));
  }
  return result;
};

const getSeparatedBasePlane = () => {
  let result = "";
  for (let i = 0xffff; i >= 0; --i) {
    result += String.fromCharCode(i) + "\n";
  }
  return result;
};

const getJoinedBasePlane = () => {
  let result = "";
  for (let i = 0; i <= 0xffff; ++i) {
    result += String.fromCharCode(i);
  }
  return result;
};

const getSurrogateCombinations = () => {
  const upperLead = String.fromCharCode(0xdbff);
  const lowerTrail = String.fromCharCode(0xdc00);

  const regularSlot = ["w", "abcdefghijklmnopqrst", "aaaaaaaaaaaaaaaaaaaa", ""];
  const surrogateSlot = [lowerTrail, upperLead];

  let samples = [];
  for (const leadSnippet of regularSlot) {
    for (const firstSlot of surrogateSlot) {
      for (const trailSnippet of regularSlot) {
        for (const secondSlot of surrogateSlot) {
          samples.push(leadSnippet + firstSlot + secondSlot + trailSnippet);
        }
        samples.push(leadSnippet + firstSlot + trailSnippet);
      }
    }
  }

  return samples;
};

const fetchFrom = async (itemKey, sample, meanwhile) => {
  const principal = getPrincipal("http://example.com/", {});

  let request = clearOrigin(principal);
  await requestFinished(request);

  const storage = getLocalStorage(principal);

  await storage.setItem(itemKey, sample);

  await meanwhile(principal);

  return storage.getItem(itemKey);
};

/**
 * Value fetched from existing snapshot based on
 * existing in-memory datastore in the parent process
 * without any communication between content/parent
 */
const fetchFromExistingSnapshotExistingDatastore = async (itemKey, sample) => {
  return fetchFrom(itemKey, sample, async () => {});
};

/**
 * Value fetched from newly created snapshot based on
 * existing in-memory datastore in the parent process
 */
const fetchFromNewSnapshotExistingDatastore = async (itemKey, sample) => {
  return fetchFrom(itemKey, sample, async () => {
    await returnToEventLoop();
  });
};

/**
 * Value fetched from newly created snapshot based on newly created
 * in-memory datastore based on database in the parent process
 */
const fetchFromNewSnapshotNewDatastore = async (itemKey, sample) => {
  return fetchFrom(itemKey, sample, async principal => {
    let request = resetOrigin(principal);
    await requestFinished(request);
  });
};

add_task(async function testSteps() {
  /* This test is based on bug 1681300 */
  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  const reportWhat = (testKey, testValue) => {
    if (testKey.length + testValue.length > 82) {
      return testKey;
    }
    return sanitizeOutputWithSurrogates(testValue, /* prefix */ testKey + ":");
  };

  const testFetchMode = async (testType, storeAndLookup) => {
    const testPairs = [
      { testEmptyValue: [""] },
      { testSampleKey: [getEncodingSample()] },
      { testSeparatedKey: [getSeparatedBasePlane()] },
      { testJoinedKey: [getJoinedBasePlane()] },
      { testCombinations: getSurrogateCombinations() },
    ];

    for (const testPair of testPairs) {
      for (const [testKey, expectedValues] of Object.entries(testPair)) {
        for (const expected of expectedValues) {
          const actual = await storeAndLookup(testKey, expected);
          const testInfo = reportWhat(testKey, expected);
          is(
            null != actual,
            true,
            testType + ": Value not null for " + testInfo
          );
          is(
            expected.length,
            actual.length,
            testType + ": Returned size for " + testInfo
          );

          const notEquals = collectCorrupted(expected, actual);
          for (let i = 0; i < notEquals.length; ++i) {
            is(
              notEquals[i][0],
              notEquals[i][1],
              testType + ": Unequal character at " + i + " for " + testInfo
            );
          }
        }
      }
    }
  };

  await testFetchMode(
    "ExistingSnapshotExistingDatastore",
    fetchFromExistingSnapshotExistingDatastore
  );

  await testFetchMode(
    "NewSnapshotExistingDatastore",
    fetchFromNewSnapshotExistingDatastore
  );

  await testFetchMode(
    "NewSnapshotNewDatastore",
    fetchFromNewSnapshotNewDatastore
  );
});
