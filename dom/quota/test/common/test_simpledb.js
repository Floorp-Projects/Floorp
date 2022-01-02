/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/common/file.js");

async function testSteps() {
  const name = "data";
  const bufferSize = 100;

  let database = getSimpleDatabase();

  let request = database.open(name);
  await requestFinished(request);

  let buffer1 = getRandomBuffer(bufferSize);

  request = database.write(buffer1);
  await requestFinished(request);

  request = database.seek(0);
  await requestFinished(request);

  request = database.read(bufferSize);
  let result = await requestFinished(request);

  let buffer2 = result.getAsArrayBuffer();

  ok(compareBuffers(buffer1, buffer2), "Buffers equal.");

  let database2 = getSimpleDatabase();

  try {
    request = database2.open(name);
    await requestFinished(request);
    ok(false, "Should have thrown!");
  } catch (ex) {
    ok(request.resultCode == NS_ERROR_STORAGE_BUSY, "Good result code.");
  }

  request = database.close();
  await requestFinished(request);

  request = database2.open(name);
  await requestFinished(request);

  request = database2.close();
  await requestFinished(request);
}
