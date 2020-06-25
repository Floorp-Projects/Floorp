/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/common/file.js");

async function testSteps() {
  // First, ensure the origin can be initialized and used by a client that uses
  // SQLite databases.

  // Todo: consider using simpleDB once it supports using storage for SQLite.
  info("Testing SQLite database with an origin that ends with a dot");

  const principal = getPrincipal("https://example.com.");
  let request = indexedDB.openForPrincipal(principal, "myIndexedDB");
  await openDBRequestUpgradeNeeded(request);

  info("Testing simple operations");

  const database = request.result;

  const objectStore = database.createObjectStore("Blobs", {});

  objectStore.add(getNullBlob(200), 42);

  await openDBRequestSucceeded(request);

  database.close();

  info("Reseting");

  request = reset();
  await requestFinished(request);

  let idbDB = getRelativeFile(
    "storage/default/https+++example.com./idb/2320029346mByDIdnedxe.sqlite"
  );
  ok(idbDB.exists(), "IDB database was created successfully");

  // Second, ensure storage initialization works fine with the origin.

  info("Testing storage initialization and temporary storage initialization");

  request = init();
  await requestFinished(request);

  request = initTemporaryStorage();
  await requestFinished(request);

  // Third, ensure QMS APIs that touch the client directory for the origin work
  // fine.

  info("Testing getUsageForPrincipal");

  request = getOriginUsage(principal);
  await requestFinished(request);

  ok(
    request.result instanceof Ci.nsIQuotaOriginUsageResult,
    "The result is nsIQuotaOriginUsageResult instance"
  );
  ok(request.result.usage > 0, "Total usage is not empty");
  ok(request.result.fileUsage > 0, "File usage is not empty");

  info("Testing clearStoragesForPrincipal");

  request = clearOrigin(principal, "default");
  await requestFinished(request);
}
