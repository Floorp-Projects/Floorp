function run_test()
{
  var url = new URL('http://www.example.com');
  do_check_eq(url.href, "http://www.example.com/");

  var url2 = new URL('/foobar', url);
  do_check_eq(url2.href, "http://www.example.com/foobar");

  // Blob is not exposed in xpcshell yet, but when it is we should
  // reenable the Blob bits here.
  do_check_false("Blob" in this)

  /*
  var blob = new Blob(['a']);
  var url = URL.createObjectURL(blob);
  ok(url, "URL is created!");

  var u = new URL(url);
  ok(u, "URL created");
  is(u.origin, "null", "Url doesn't have an origin if created in a JSM");

  URL.revokeObjectURL(url);
  ok(true, "URL is revoked");
  */
}
