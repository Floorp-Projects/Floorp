this.EXPORTED_SYMBOLS = ['checkFromJSM'];

this.checkFromJSM = function checkFromJSM(ok, is) {
  Components.utils.importGlobalProperties(['URL']);

  var url = new URL('http://www.example.com');
  is(url.href, "http://www.example.com/", "JSM should have URL");

  var url2 = new URL('/foobar', url);
  is(url2.href, "http://www.example.com/foobar", "JSM should have URL - based on another URL");

  var blob = new Blob(['a']);
  var url = URL.createObjectURL(blob);
  ok(url, "URL is created!");

  var p = URL.getPrincipalFromURL(url);
  ok(p, "Principal exists.");
  ok(p instanceof Components.interfaces.nsIPrincipal, "Principal is a nsIPrincipal");

  URL.revokeObjectURL(url);
  ok(true, "URL is revoked");
}
