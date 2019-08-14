// Bug 380852 - Delete permission manager entries in Clear Recent History

function countPermissions() {
  let result = 0;
  let enumerator = Services.perms.enumerator;
  while (enumerator.hasMoreElements()) {
    result++;
    enumerator.getNext();
  }
  return result;
}

add_task(async function test() {
  // sanitize before we start so we have a good baseline.
  await Sanitizer.sanitize(["siteSettings"], { ignoreTimespan: false });

  // Count how many permissions we start with - some are defaults that
  // will not be sanitized.
  let numAtStart = countPermissions();

  // Add a permission entry
  var pm = Services.perms;
  pm.add(Services.io.newURI("http://example.com"), "testing", pm.ALLOW_ACTION);

  // Sanity check
  ok(
    pm.enumerator.hasMoreElements(),
    "Permission manager should have elements, since we just added one"
  );

  // Clear it
  await Sanitizer.sanitize(["siteSettings"], { ignoreTimespan: false });

  // Make sure it's gone
  is(
    numAtStart,
    countPermissions(),
    "Permission manager should have the same count it started with"
  );
});
