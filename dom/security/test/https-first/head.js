registerCleanupFunction(async function () {
  Services.perms.removeByType("https-only-load-insecure");
});
