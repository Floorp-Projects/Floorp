/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test repeatedly setting values that are just under the LocalStorage quota
 * limit without yielding control flow in order to verify that the write
 * optimizer is present / works. If there was no write optimizer present, the
 * IPC message size limit would be exceeded, resulting in a crash.
 */

add_task(async function testSteps() {
  const globalLimitKB = 5 * 1024;

  // 18 and more iterations would produce an IPC message with size greater than
  // 256 MB if write optimizer was not present. This number was determined
  // experimentally by running the test with disabled write optimizer.
  const numberOfIterations = 18;

  const randomStringBlockSize = 65536;

  // We need to use a random string because LS internally tries to compress
  // values.
  function getRandomString(size) {
    let crypto = this.window ? this.window.crypto : this.crypto;
    let decoder = new TextDecoder("ISO-8859-2");

    function getRandomStringBlock(array) {
      crypto.getRandomValues(array);
      return decoder.decode(array);
    }

    let string = "";

    let quotient = size / randomStringBlockSize;
    if (quotient) {
      let array = new Uint8Array(randomStringBlockSize);
      for (let i = 1; i <= quotient; i++) {
        string += getRandomStringBlock(array);
      }
    }

    let remainder = size % randomStringBlockSize;
    if (remainder) {
      let array = new Uint8Array(remainder);
      string += getRandomStringBlock(array);
    }

    return string;
  }

  const data = {};
  data.key = "foo";
  data.value = getRandomString(
    globalLimitKB * 1024 -
      data.key.length -
      numberOfIterations.toString().length
  );

  info("Setting pref");

  // By disabling snapshot reusing, we guarantee that the snapshot will be
  // checkpointed once control returns to the event loop.
  if (this.window) {
    await SpecialPowers.pushPrefEnv({
      set: [["dom.storage.snapshot_reusing", false]],
    });
  } else {
    Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);
  }

  info("Getting storage");

  let storage = getLocalStorage();

  info("Adding/updating item");

  for (var i = 0; i < numberOfIterations; i++) {
    storage.setItem(data.key, data.value + i);
  }

  info("Returning to event loop");

  await returnToEventLoop();

  ok(!storage.hasSnapshot, "Snapshot successfully finished");
});
