/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that initializePersistentClient() does call
 * QuotaManager::EnsurePersistentClientIsInitialized() which ensures client
 * directory existence.
 */

async function testSteps() {
  const clientMetadata = {
    persistence: "default",
    principal: getPrincipal("https://foo.example.com"),
    client: "sdb",
    file: getRelativeFile("storage/permanent/https+++foo.example.com/sdb"),
  };

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Initializing");

  request = init();
  await requestFinished(request);

  info("Initializing persistent origin");

  request = initPersistentOrigin(clientMetadata.principal);
  await requestFinished(request);

  ok(!clientMetadata.file.exists(), "Client directory does not exist");

  info("Initializing persistent client");

  request = initPersistentClient(
    clientMetadata.principal,
    clientMetadata.client
  );
  await requestFinished(request);

  ok(clientMetadata.file.exists(), "Client directory does exist");
}
