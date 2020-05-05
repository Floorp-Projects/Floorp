/**
 * The schema_25_profile.zip are made from local Nightly by following step
 * 1. Go to any website
 * 2. Open web console and type
 *      caches.open("test")
 *        .then(c => fetch("https://www.mozilla.org", {mode:"no-cors"})
 *        .then(r => c.put("https://www.mozilla.org", r)));
 * 3. Go to profile directory and rename the website folder to "chrome"
 */

async function testSteps() {
  create_test_profile("schema_25_profile.zip");

  let cache = await caches.open("test");
  let response = await cache.match("https://www.mozilla.org");
  ok(!!response, "Upgrade from 25 to 26 do succeed");
  ok(response.type === "opaque", "The response type does be opaque");
}
