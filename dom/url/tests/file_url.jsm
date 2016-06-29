this.EXPORTED_SYMBOLS = ['checkFromJSM'];

this.checkFromJSM = function checkFromJSM(ok, is) {
  Components.utils.importGlobalProperties(['URL', 'Blob']);

  var url = new URL('http://www.example.com');
  is(url.href, "http://www.example.com/", "JSM should have URL");

  var url2 = new URL('/foobar', url);
  is(url2.href, "http://www.example.com/foobar", "JSM should have URL - based on another URL");

  var blob = new Blob(['a']);
  var url = URL.createObjectURL(blob);
  ok(url, "URL is created!");

  var u = new URL(url);
  ok(u, "URL created");
  is(u.origin, "null", "Url doesn't have an origin if created in a JSM");

  URL.revokeObjectURL(url);
  ok(true, "URL is revoked");
}
