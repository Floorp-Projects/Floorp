let cache;
let url = 'foo.html';
let redirectURL = 'http://example.com/foo-bar.html';
caches.open('redirect-' + context).then(c => {
  cache = c;
  var response = Response.redirect(redirectURL);
  is(response.headers.get('Location'), redirectURL);
  return cache.put(url, response);
}).then(_ => {
  return cache.match(url);
}).then(response => {
  is(response.headers.get('Location'), redirectURL);
  testDone();
});
