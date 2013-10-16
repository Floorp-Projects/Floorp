function ok(a, msg) {
  postMessage({type: 'status', status: !!a, msg: msg });
}

onmessage = function(event) {
  // URL.href throws
  var url = new URL('http://www.example.com');
  ok(url, "URL created");

  var status = false;
  try {
    url.href = '42';
  } catch(e) {
    status = true;
  }
  ok(status, "url.href = 42 should throw");

  url.href = 'http://www.example.org';
  ok(true, "url.href should not throw");

  status = false
  try {
    new URL('42');
  } catch(e) {
    status = true;
  }
  ok(status, "new URL(42) should throw");

  status = false
  try {
    new URL('http://www.example.com', '42');
  } catch(e) {
    status = true;
  }
  ok(status, "new URL(something, 42) should throw");

  postMessage({type: 'finish' });
}
