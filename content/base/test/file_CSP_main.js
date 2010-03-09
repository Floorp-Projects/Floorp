// some javascript for the CSP XHR tests
//

try {
  var xhr_good = new XMLHttpRequest();
  var xhr_good_uri ="http://mochi.test:8888/tests/content/base/test/file_CSP.sjs?testid=xhr_good";
  xhr_good.open("GET", xhr_good_uri, true);
  xhr_good.send(null);
} catch(e) {}

try {
  var xhr_bad = new XMLHttpRequest();
  var xhr_bad_uri ="http://example.com/tests/content/base/test/file_CSP.sjs?testid=xhr_bad";
  xhr_bad.open("GET", xhr_bad_uri, true);
  xhr_bad.send(null);
} catch(e) {}
