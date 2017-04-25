/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from ../file.js */

var disableWorkerTest = "FileHandle doesn't work in workers yet";

var testGenerator = testSteps();

function* testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  var testString = "Lorem ipsum his ponderum delicatissimi ne, at noster dolores urbanitas pro, cibo elaboraret no his. Ea dicunt maiorum usu. Ad appareat facilisis mediocritatem eos. Tale graeci mentitum in eos, hinc insolens at nam. Graecis nominavi aliquyam eu vix. Id solet assentior sadipscing pro. Et per atqui graecis, usu quot viris repudiandae ei, mollis evertitur an nam. At nam dolor ignota, liber labore omnesque ea mei, has movet voluptaria in. Vel an impetus omittantur. Vim movet option salutandi ex, ne mei ignota corrumpit. Mucius comprehensam id per. Est ea putant maiestatis.";
  for (let i = 0; i < 5; i++) {
    testString += testString;
  }

  var testBuffer = getRandomBuffer(100000);

  var testBlob = new Blob([testBuffer], {type: "binary/random"});

  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;

  request = db.createMutableFile("test.txt");
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let mutableFile = event.target.result;
  mutableFile.onerror = errorHandler;

  let location = 0;

  let fileHandle = mutableFile.open("readwrite");
  is(fileHandle.location, location, "Correct location");

  request = fileHandle.append(testString);
  ok(fileHandle.location === null, "Correct location");
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  fileHandle.location = 0;
  request = fileHandle.readAsText(testString.length);
  location += testString.length
  is(fileHandle.location, location, "Correct location");
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let resultString = event.target.result;
  ok(resultString == testString, "Correct string data");

  request = fileHandle.append(testBuffer);
  ok(fileHandle.location === null, "Correct location");
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  fileHandle.location = location;
  request = fileHandle.readAsArrayBuffer(testBuffer.byteLength);
  location += testBuffer.byteLength;
  is(fileHandle.location, location, "Correct location");
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let resultBuffer = event.target.result;
  ok(compareBuffers(resultBuffer, testBuffer), "Correct array buffer data");

  request = fileHandle.append(testBlob);
  ok(fileHandle.location === null, "Correct location");
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  fileHandle.location = location;
  request = fileHandle.readAsArrayBuffer(testBlob.size);
  location += testBlob.size;
  is(fileHandle.location, location, "Correct location");
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  resultBuffer = event.target.result;
  ok(compareBuffers(resultBuffer, testBuffer), "Correct blob data");

  request = fileHandle.getMetadata({ size: true });
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  let result = event.target.result;
  is(result.size, location, "Correct size");

  finishTest();
}
