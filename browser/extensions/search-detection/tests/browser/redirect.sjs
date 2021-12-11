const REDIRECT_SJS =
  "browser/browser/extensions/search-detection/tests/browser/redirect.sjs";

// This handler is used to create redirect chains with multiple sub-domains,
// and the next hop is defined by the current `host`.
function handleRequest(request, response) {
  let newLocation;

  // test1.example.com -> test2.example.com -> www.example.com -> example.net
  switch (request.host) {
    case "test1.example.com":
      newLocation = `https://test2.example.com/${REDIRECT_SJS}`;
      break;
    case "test2.example.com":
      newLocation = `https://www.example.com/${REDIRECT_SJS}`;
      break;
    case "www.example.com":
      newLocation = "https://example.net/";
      break;
    // We redirect `mochi.test` to `www` in
    // `test_redirect_chain_does_not_start_on_first_request()`.
    case "mochi.test":
      newLocation = `https://www.example.com/${REDIRECT_SJS}`;
      break;
    default:
      // Redirect to a different website in case of unexpected events.
      newLocation = "https://mozilla.org/";
  }

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", newLocation);
}
