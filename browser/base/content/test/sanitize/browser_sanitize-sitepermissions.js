// Bug 380852 - Delete permission manager entries in Clear Recent History

function countPermissions() {
  return Services.perms.all.length;
}

add_task(async function test() {
  // sanitize before we start so we have a good baseline.
  await Sanitizer.sanitize(["siteSettings"], { ignoreTimespan: false });

  // Count how many permissions we start with - some are defaults that
  // will not be sanitized.
  let numAtStart = countPermissions();

  // Add a permission entry
  PermissionTestUtils.add(
    "https://example.com",
    "testing",
    Services.perms.ALLOW_ACTION
  );

  // Sanity check
  ok(
    !!Services.perms.all.length,
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
