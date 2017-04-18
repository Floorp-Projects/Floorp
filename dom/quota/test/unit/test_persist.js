/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const origins = [
    {
      url: "http://default.test.persist",
      path: "storage/default/http+++default.test.persist",
      persistence: "default"
    },

    {
      url: "ftp://ftp.invalid.origin",
      path: "storage/default/ftp+++ftp.invalid.origin",
      persistence: "default"
    },
  ];

  const metadataFileName = ".metadata-v2";

  let principal = getPrincipal(origins[0].url);

  info("Persisting an uninitialized origin");

  // Origin directory doesn't exist yet, so only check the result for
  // persisted().
  let request = persisted(principal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persisted() succeeded");
  ok(!request.result, "The origin is not persisted");

  info("Verifying persist() does update the metadata");

  request = persist(principal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persist() succeeded");

  let originDir = getRelativeFile(origins[0].path);
  let exists = originDir.exists();
  ok(exists, "Origin directory does exist");

  info("Reading out contents of metadata file");

  let metadataFile = originDir.clone();
  metadataFile.append(metadataFileName);

  File.createFromNsIFile(metadataFile).then(grabArgAndContinueHandler);
  let file = yield undefined;

  let fileReader = new FileReader();
  fileReader.onload = continueToNextStepSync;
  fileReader.readAsArrayBuffer(file);
  yield undefined;

  let originPersisted = getPersistedFromMetadata(fileReader.result);
  ok(originPersisted, "The origin is persisted");

  info("Verifying persisted()");

  request = persisted(principal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persisted() succeeded");
  ok(request.result === originPersisted, "Persisted() concurs with metadata");

  info("Clearing the origin");

  // Clear the origin since we'll test the same directory again under different
  // circumstances.
  clearOrigin(principal, origins[0].persistence, continueToNextStepSync);
  yield undefined;

  info("Persisting an already initialized origin");

  initOrigin(principal, origins[0].persistence, continueToNextStepSync);
  yield undefined;

  info("Reading out contents of metadata file");

  fileReader = new FileReader();
  fileReader.onload = continueToNextStepSync;
  fileReader.readAsArrayBuffer(file);
  yield undefined;

  originPersisted = getPersistedFromMetadata(fileReader.result);
  ok(!originPersisted, "The origin isn't persisted after clearing");

  info("Verifying persisted()");

  request = persisted(principal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persisted() succeeded");
  ok(request.result === originPersisted, "Persisted() concurs with metadata");

  info("Verifying persist() does update the metadata");

  request = persist(principal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persist() succeeded");

  info("Reading out contents of metadata file");

  fileReader = new FileReader();
  fileReader.onload = continueToNextStepSync;
  fileReader.readAsArrayBuffer(file);
  yield undefined;

  originPersisted = getPersistedFromMetadata(fileReader.result);
  ok(originPersisted, "The origin is persisted");

  info("Verifying persisted()");

  request = persisted(principal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persisted() succeeded");
  ok(request.result === originPersisted, "Persisted() concurs with metadata");

  info("Persisting an invalid origin");

  let invalidPrincipal = getPrincipal(origins[1].url);

  request = persist(invalidPrincipal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_ERROR_FAILURE,
     "Persist() failed because of the invalid origin");
  ok(request.result === null, "The request result is null");

  originDir = getRelativeFile(origins[1].path);
  exists = originDir.exists();
  ok(!exists, "Directory for invalid origin doesn't exist");

  request = persisted(invalidPrincipal, continueToNextStepSync);
  yield undefined;

  ok(request.resultCode === NS_OK, "Persisted() succeeded");
  ok(!request.result,
     "The origin isn't persisted since the operation failed");

  finishTest();
}
