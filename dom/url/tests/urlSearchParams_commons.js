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

  is(u + "", "foo=bar2", "stringifier");

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

function testURL() {
  var url = new URL('http://www.example.net?a=b&c=d');
  ok(url.searchParams, "URL searchParams exists!");
  ok(url.searchParams.has('a'), "URL.searchParams.has('a')");
  is(url.searchParams.get('a'), 'b', "URL.searchParams.get('a')");
  ok(url.searchParams.has('c'), "URL.searchParams.has('c')");
  is(url.searchParams.get('c'), 'd', "URL.searchParams.get('c')");

  url.searchParams.set('e', 'f');
  ok(url.href.indexOf('e=f') != 1, 'URL right');


  url = new URL('mailto:a@b.com?subject=Hi');
  ok(url.searchParams, "URL searchParams exists!");
  ok(url.searchParams.has('subject'), "Hi");

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
    { input: '?a=1', data: { 'a' : ['1'] } },
    { input: '?', data: {} },
    { input: '?=b', data: { '' : ['b'] } },
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
    var url = new URL('http://www.example.net');
    url.searchParams.set('a', encoding[i][0]);
    is(url.href, 'http://www.example.net/?a=' + encoding[i][1]);

    var url2 = new URL(url.href);
    is(url2.searchParams.get('a'), encoding[i][0], 'a is still there');
  }

  runTest();
}

function testCopyConstructor() {
  var url = new URL("http://example.com/");
  var p = url.searchParams;
  var q = new URLSearchParams(p);
  q.set("a", "b");
  is(url.href, "http://example.com/",
     "Messing with copy of URLSearchParams should not affect URL");
  p.set("c", "d");
  is(url.href, "http://example.com/?c=d",
     "Messing with URLSearchParams should affect URL");

  runTest();
}

function testOrdering() {
  var a = new URLSearchParams("a=1&a=2&b=3&c=4&c=5&a=6");
  is(a.toString(), "a=1&a=2&b=3&c=4&c=5&a=6", "Order is correct");
  is(a.getAll('a').length, 3, "Correct length of getAll()");

  var b = new URLSearchParams();
  b.append('a', '1');
  b.append('b', '2');
  b.append('a', '3');
  is(b.toString(), "a=1&b=2&a=3", "Order is correct");
  is(b.getAll('a').length, 2, "Correct length of getAll()");

  runTest();
}

function testDelete() {
  var a = new URLSearchParams("a=1&a=2&b=3&c=4&c=5&a=6");
  is(a.toString(), "a=1&a=2&b=3&c=4&c=5&a=6", "Order is correct");
  is(a.getAll('a').length, 3, "Correct length of getAll()");

  a.delete('a');
  is(a.getAll('a').length, 0, "Correct length of getAll()");
  is(a.toString(), "b=3&c=4&c=5", "Order is correct");

  runTest();
}

function testGetNULL() {
  var u = new URLSearchParams();
  is(typeof u.get(''), "object", "typeof URL.searchParams.get('')");
  is(u.get(''), null, "URL.searchParams.get('') should be null");

  var url = new URL('http://www.example.net?a=b');
  is(url.searchParams.get('b'), null, "URL.searchParams.get('b') should be null");
  is(url.searchParams.get('a'), 'b', "URL.searchParams.get('a')");

  runTest();
}

function testSet() {
  var u = new URLSearchParams();
  u.set('a','b');
  u.set('e','c');
  u.set('i','d');
  u.set('o','f');
  u.set('u','g');

  is(u.get('a'), 'b', "URL.searchParams.get('a') should return b");
  is(u.getAll('a').length, 1, "URLSearchParams.getAll('a').length should be 1");

  u.set('a','h1');
  u.set('a','h2');
  u.set('a','h3');
  u.set('a','h4');
  is(u.get('a'), 'h4', "URL.searchParams.get('a') should return h4");
  is(u.getAll('a').length, 1, "URLSearchParams.getAll('a').length should be 1");

  is(u.get('e'), 'c', "URL.searchParams.get('e') should return c");
  is(u.get('i'), 'd', "URL.searchParams.get('i') should return d");
  is(u.get('o'), 'f', "URL.searchParams.get('o') should return f");
  is(u.get('u'), 'g', "URL.searchParams.get('u') should return g");

  is(u.getAll('e').length, 1, "URLSearchParams.getAll('e').length should be 1");
  is(u.getAll('i').length, 1, "URLSearchParams.getAll('i').length should be 1");
  is(u.getAll('o').length, 1, "URLSearchParams.getAll('o').length should be 1");
  is(u.getAll('u').length, 1, "URLSearchParams.getAll('u').length should be 1");

  u = new URLSearchParams("name1=value1&name1=value2&name1=value3");
  is(u.get('name1'), 'value1', "URL.searchParams.get('name1') should return value1");
  is(u.getAll('name1').length, 3, "URLSearchParams.getAll('name1').length should be 3");
  u.set('name1','firstPair');
  is(u.get('name1'), 'firstPair', "URL.searchParams.get('name1') should return firstPair");
  is(u.getAll('name1').length, 1, "URLSearchParams.getAll('name1').length should be 1");

  runTest();
}

function testIterable() {
  var u = new URLSearchParams();
  u.set('1','2');
  u.set('2','4');
  u.set('3','6');
  u.set('4','8');
  u.set('5','10');

  var key_iter = u.keys();
  var value_iter = u.values();
  var entries_iter = u.entries();
  for (var i = 0; i < 5; ++i) {
    var v = i + 1;
    var key = key_iter.next();
    var value = value_iter.next();
    var entry = entries_iter.next();
    is(key.value, v.toString(), "Correct Key iterator: " + v.toString());
    ok(!key.done, "Key.done is false");
    is(value.value, (v * 2).toString(), "Correct Value iterator: " + (v * 2).toString());
    ok(!value.done, "Value.done is false");
    is(entry.value[0], v.toString(), "Correct Entry 0 iterator: " + v.toString());
    is(entry.value[1], (v * 2).toString(), "Correct Entry 1 iterator: " + (v * 2).toString());
    ok(!entry.done, "Entry.done is false");
  }

  var last = key_iter.next();
  ok(last.done, "Nothing more to read.");
  is(last.value, undefined, "Undefined is the last key");

  last = value_iter.next();
  ok(last.done, "Nothing more to read.");
  is(last.value, undefined, "Undefined is the last value");

  last = entries_iter.next();
  ok(last.done, "Nothing more to read.");

  key_iter = u.keys();
  key_iter.next();
  key_iter.next();
  u.delete('1');
  u.delete('2');
  u.delete('3');
  u.delete('4');
  u.delete('5');

  last = key_iter.next();
  ok(last.done, "Nothing more to read.");
  is(last.value, undefined, "Undefined is the last key");

  runTest();
}
function testZeroHandling() {
  var u = new URLSearchParams;
  u.set("a", "b\0c");
  u.set("d\0e", "f");
  u.set("g\0h", "i\0j");
  is(u.toString(), "a=b%00c&d%00e=f&g%00h=i%00j",
     "Should encode U+0000 as %00");

  runTest();
}

function testCTORs() {
  var a = new URLSearchParams("a=b");
  is(a.get("a"), "b", "CTOR with string");

  var b = new URLSearchParams([['a', 'b'], ['c', 'd']]);
  is(b.get("a"), "b", "CTOR with sequence");
  is(b.get("c"), "d", "CTOR with sequence");

  ok(new URLSearchParams([]), "CTOR with empty sequence");

  let result;
  try {
    result = new URLSearchParams([[1]]);
  } catch(e) {
    result = 42;
  }

  is(result, 42, "CTOR throws if the sequence doesn't contain exactly 2 elements");

  try {
    result = new URLSearchParams([[1,2,3]]);
  } catch(e) {
    result = 43;
  }
  is(result, 43, "CTOR throws if the sequence doesn't contain exactly 2 elements");

  var c = new URLSearchParams({ a: 'b', c: 42, d: null, e: [1,2,3], f: {a:42} });
  is(c.get('a'), 'b', "CTOR with record<>");
  is(c.get('c'), '42', "CTOR with record<>");
  is(c.get('d'), 'null', "CTOR with record<>");
  is(c.get('e'), [1,2,3].toString(), "CTOR with record<>");
  is(c.get('f'), {a:42}.toString(), "CTOR with record<>");

  runTest();
}
