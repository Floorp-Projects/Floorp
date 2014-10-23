function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

onmessage = function() {
  status = false;
  try {
    if ((URLSearchParams instanceof Object)) {
      status = true;
    }
  } catch(e) {
  }
  ok(status, "URLSearchParams in workers \\o/");

  function testSimpleURLSearchParams() {
    var u = new URLSearchParams();
    ok(u, "URLSearchParams created");
    is(u.has('foo'), false, 'URLSearchParams.has(foo)');
    is(u.get('foo'), null, 'URLSearchParams.get(foo)');
    is(u.getAll('foo').length, 0, 'URLSearchParams.getAll(foo)');

    u.append('foo', 'bar');
    is(u.has('foo'), true, 'URLSearchParams.has(foo)');
    is(u.get('foo'), 'bar', 'URLSearchParams.get(foo)');
    is(u.getAll('foo').length, 1, 'URLSearchParams.getAll(foo)');

    u.set('foo', 'bar2');
    is(u.get('foo'), 'bar2', 'URLSearchParams.get(foo)');
    is(u.getAll('foo').length, 1, 'URLSearchParams.getAll(foo)');

    is(u + "", "foo=bar2", "stringify");

    u.delete('foo');

    runTest();
  }

  function testCopyURLSearchParams() {
    var u = new URLSearchParams();
    ok(u, "URLSearchParams created");
    u.append('foo', 'bar');

    var uu = new URLSearchParams(u);
    is(uu.get('foo'), 'bar', 'uu.get()');

    u.append('foo', 'bar2');
    is(u.getAll('foo').length, 2, "u.getAll()");
    is(uu.getAll('foo').length, 1, "uu.getAll()");

    runTest();
  }

  function testParserURLSearchParams() {
    var checks = [
      { input: '', data: {} },
      { input: 'a', data: { 'a' : [''] } },
      { input: 'a=b', data: { 'a' : ['b'] } },
      { input: 'a=', data: { 'a' : [''] } },
      { input: '=b', data: { '' : ['b'] } },
      { input: '&', data: {} },
      { input: '&a', data: { 'a' : [''] } },
      { input: 'a&', data: { 'a' : [''] } },
      { input: 'a&a', data: { 'a' : ['', ''] } },
      { input: 'a&b&c', data: { 'a' : [''], 'b' : [''], 'c' : [''] } },
      { input: 'a=b&c=d', data: { 'a' : ['b'], 'c' : ['d'] } },
      { input: 'a=b&c=d&', data: { 'a' : ['b'], 'c' : ['d'] } },
      { input: '&&&a=b&&&&c=d&', data: { 'a' : ['b'], 'c' : ['d'] } },
      { input: 'a=a&a=b&a=c', data: { 'a' : ['a', 'b', 'c'] } },
      { input: 'a==a', data: { 'a' : ['=a'] } },
      { input: 'a=a+b+c+d', data: { 'a' : ['a b c d'] } },
      { input: '%=a', data: { '%' : ['a'] } },
      { input: '%a=a', data: { '%a' : ['a'] } },
      { input: '%a_=a', data: { '%a_' : ['a'] } },
      { input: '%61=a', data: { 'a' : ['a'] } },
      { input: '%=a', data: { '%' : ['a'] } },
      { input: '%a=a', data: { '%a' : ['a'] } },
      { input: '%a_=a', data: { '%a_' : ['a'] } },
      { input: '%61=a', data: { 'a' : ['a'] } },
      { input: '%61+%4d%4D=', data: { 'a MM' : [''] } },
    ];

    for (var i = 0; i < checks.length; ++i) {
      var u = new URLSearchParams(checks[i].input);

      var count = 0;
      for (var key in checks[i].data) {
        ++count;
        ok(u.has(key), "key " + key + " found");

        var all = u.getAll(key);
        is(all.length, checks[i].data[key].length, "same number of elements");

        for (var k = 0; k < all.length; ++k) {
          is(all[k], checks[i].data[key][k], "value matches");
        }
      }
    }

    runTest();
  }

  function testEncoding() {
    var encoding = [ [ '1', '1' ],
                     [ 'a b', 'a+b' ],
                     [ '<>', '%3C%3E' ],
                     [ '\u0541', '%D5%81'] ];

    for (var i = 0; i < encoding.length; ++i) {
      var a = new URLSearchParams();
      a.set('a', encoding[i][0]);
      is(a + "", 'a=' + encoding[i][1]);
    }

    runTest();
  }

  var tests = [
    testSimpleURLSearchParams,
    testCopyURLSearchParams,
    testParserURLSearchParams,
    testEncoding
  ];

  function runTest() {
    if (!tests.length) {
      postMessage({type: 'finish' });
      return;
    }

    var test = tests.shift();
    test();
  }

  runTest();
}
