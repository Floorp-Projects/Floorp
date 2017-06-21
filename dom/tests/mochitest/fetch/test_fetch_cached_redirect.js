async function testCachedRedirectErrorMode() {
  // This is a file that returns a 302 to someplace else and will be cached.
  const REDIRECTING_URL = "file_fetch_cached_redirect.html";

  let firstResponse = await(fetch(REDIRECTING_URL, { redirect: "manual" }));
  // okay, now it should be in the cahce.
  try {
    let secondResponse = await(fetch(REDIRECTING_URL, { redirect: "error" }));
  } catch(ex) {

  }

  ok(true, "didn't crash");
}

function runTest() {
  return Promise.resolve()
    .then(testCachedRedirectErrorMode)
    // Put more promise based tests here.
}
