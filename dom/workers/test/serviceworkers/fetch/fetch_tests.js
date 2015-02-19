function fetch(name, onload, onerror, headers) {
  expectAsyncResult();

  onload = onload || function() {
    my_ok(false, "XHR load should not complete successfully");
    finish();
  };
  onerror = onerror || function() {
    my_ok(false, "XHR load should be intercepted successfully");
    finish();
  };

  var x = new XMLHttpRequest();
  x.open('GET', name, true);
  x.onload = function() { onload(x) };
  x.onerror = function() { onerror(x) };
  headers = headers || [];
  headers.forEach(function(header) {
    x.setRequestHeader(header[0], header[1]);
  });
  x.send();
}

fetch('synthesized.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "synthesized response body", "load should have synthesized response");
  finish();
});

fetch('ignored.txt', function(xhr) {
  my_ok(xhr.status == 404, "load should be uninterrupted");
  finish();
});

fetch('rejected.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetch('nonresponse.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetch('nonresponse2.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetch('headers.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "1", "request header checks should have passed");
  finish();
}, null, [["X-Test1", "header1"], ["X-Test2", "header2"]]);
