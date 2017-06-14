
// The xml serializer uses the default line break of the plateform.
// So we need to know the value of this default line break, in order
// to build correctly the reference strings for tests.
// This variable will contain this value.
var LB;

function run_test() {

  if (mozinfo.os == "win") {
    LB = "\r\n";
  } else {
    LB = "\n";
  }

  for (var i = 0; i < tests.length && tests[i]; ++i) {
    tests[i].call();
  }
}

var tests = [
  test1,
  test2,
  test3,
  test4,
  test5,
  test6,
  test7,
  test8,
  test9,
  test10,
  null
];

function testString(str) {
  do_check_eq(roundtrip(str), str);
}

function test1() {
  // Basic round-tripping which we expect to hand back the same text
  // as we passed in (not strictly required for correctness in some of
  // those cases, but best for readability of serializer output)
  testString('<root/>');
  testString('<root><child/></root>');
  testString('<root xmlns=""/>');
  testString('<root xml:lang="en"/>');
  testString('<root xmlns="ns1"><child xmlns="ns2"/></root>')
  testString('<root xmlns="ns1"><child xmlns=""/></root>')
  testString('<a:root xmlns:a="ns1"><child/></a:root>')
  testString('<a:root xmlns:a="ns1"><a:child/></a:root>')
  testString('<a:root xmlns:a="ns1"><b:child xmlns:b="ns1"/></a:root>')
  testString('<a:root xmlns:a="ns1"><a:child xmlns:a="ns2"/></a:root>')
  testString('<a:root xmlns:a="ns1"><b:child xmlns:b="ns1" b:attr=""/></a:root>')
}

function test2() {
  // Test setting of "xmlns" attribute in the null namespace

  // XXXbz are these tests needed?  What should happen here?  These
  // may be bogus.

  // Setting random "xmlns" attribute
  var doc = ParseXML('<root xmlns="ns1"/>');
  doc.documentElement.setAttribute("xmlns", "ns2");
  do_check_serialize(doc);
}

function test3() {
  // Test basic appending of kids.  Again, we're making assumptions
  // about how our serializer will serialize simple DOMs.
  var doc = ParseXML('<root xmlns="ns1"/>');
  var root = doc.documentElement;
  var child = doc.createElementNS("ns2", "child");
  root.appendChild(child);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1"><child xmlns="ns2"/></root>');

  doc = ParseXML('<root xmlns="ns1"/>');
  root = doc.documentElement;
  child = doc.createElementNS("ns2", "prefix:child");
  root.appendChild(child);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1"><prefix:child xmlns:prefix="ns2"/></root>');

  doc = ParseXML('<prefix:root xmlns:prefix="ns1"/>');
  root = doc.documentElement;
  child = doc.createElementNS("ns2", "prefix:child");
  root.appendChild(child);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<prefix:root xmlns:prefix="ns1"><a0:child xmlns:a0="ns2"/>'+
              '</prefix:root>');

}

function test4() {
  // setAttributeNS tests

  var doc = ParseXML('<root xmlns="ns1"/>');
  var root = doc.documentElement;
  root.setAttributeNS("ns1", "prefix:local", "val");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1" prefix:local="val" xmlns:prefix="ns1"/>');

  doc = ParseXML('<prefix:root xmlns:prefix="ns1"/>');
  root = doc.documentElement;
  root.setAttributeNS("ns1", "local", "val");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<prefix:root xmlns:prefix="ns1" prefix:local="val"/>');

  doc = ParseXML('<root xmlns="ns1"/>');
  root = doc.documentElement;
  root.setAttributeNS("ns2", "local", "val");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1" a0:local="val" xmlns:a0="ns2"/>');

  // Handling of prefix-generation for non-null-namespace attributes
  // which have the same namespace as the current default namespace
  // (bug 301260).
  doc = ParseXML('<root xmlns="ns1"/>');
  root = doc.documentElement;
  root.setAttributeNS("ns1", "local", "val");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
                '<root xmlns="ns1" a0:local="val" xmlns:a0="ns1"/>');

  // Tree-walking test
  doc = ParseXML('<root xmlns="ns1" xmlns:a="ns2">'+
                 '<child xmlns:b="ns2" xmlns:a="ns3">'+
                 '<child2/></child></root>');
  root = doc.documentElement;
  // Have to QI here -- no classinfo flattening in xpcshell, apparently
  var node = root.firstChild.firstChild.QueryInterface(nsIDOMElement);
  node.setAttributeNS("ns4", "l1", "v1");
  node.setAttributeNS("ns4", "p2:l2", "v2");
  node.setAttributeNS("", "l3", "v3");
  node.setAttributeNS("ns3", "l4", "v4");
  node.setAttributeNS("ns3", "p5:l5", "v5");
  node.setAttributeNS("ns3", "a:l6", "v6");
  node.setAttributeNS("ns2", "l7", "v7");
  node.setAttributeNS("ns2", "p8:l8", "v8");
  node.setAttributeNS("ns2", "b:l9", "v9");
  node.setAttributeNS("ns2", "a:l10", "v10");
  node.setAttributeNS("ns1", "a:l11", "v11");
  node.setAttributeNS("ns1", "b:l12", "v12");
  node.setAttributeNS("ns1", "l13", "v13");
  do_check_serialize(doc);
  //  Note: we end up with "a2" as the prefix on "l11" and "l12" because we use
  //  "a1" earlier, and discard it in favor of something we get off the
  //  namespace stack, apparently
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1" xmlns:a="ns2">'+
              '<child xmlns:b="ns2" xmlns:a="ns3">'+
              '<child2 a0:l1="v1" xmlns:a0="ns4"' +
              ' a0:l2="v2"' +
              ' l3="v3"' +
              ' a:l4="v4"' +
              ' a:l5="v5"' +
              ' a:l6="v6"' +
              ' b:l7="v7"' +
              ' b:l8="v8"' +
              ' b:l9="v9"' +
              ' b:l10="v10"' +
              ' a2:l11="v11" xmlns:a2="ns1"' +
              ' a2:l12="v12"' +
              ' a2:l13="v13"/></child></root>');
}

function test5() {
  // Handling of kids in the null namespace when the default is a
  // different namespace (bug 301260).
  var doc = ParseXML('<root xmlns="ns1"/>')
  var child = doc.createElement('child');
  doc.documentElement.appendChild(child);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1"><child xmlns=""/></root>');
}

function test6() {
  // Handling of not using a namespace prefix (or default namespace!)
  // that's not bound to our namespace in our scope (bug 301260).
  var doc = ParseXML('<prefix:root xmlns:prefix="ns1"/>');
  var root = doc.documentElement;
  var child1 = doc.createElementNS("ns2", "prefix:child1");
  var child2 = doc.createElementNS("ns1", "prefix:child2");
  child1.appendChild(child2);
  root.appendChild(child1);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<prefix:root xmlns:prefix="ns1"><a0:child1 xmlns:a0="ns2">'+
              '<prefix:child2/></a0:child1></prefix:root>');

  doc = ParseXML('<root xmlns="ns1"><prefix:child1 xmlns:prefix="ns2"/></root>');
  root = doc.documentElement;
  child1 = root.firstChild;
  child2 = doc.createElementNS("ns1", "prefix:child2");
  child1.appendChild(child2);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="ns1"><prefix:child1 xmlns:prefix="ns2">'+
              '<child2/></prefix:child1></root>');

  doc = ParseXML('<prefix:root xmlns:prefix="ns1">'+
                 '<prefix:child1 xmlns:prefix="ns2"/></prefix:root>');
  root = doc.documentElement;
  child1 = root.firstChild;
  child2 = doc.createElementNS("ns1", "prefix:child2");
  child1.appendChild(child2);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<prefix:root xmlns:prefix="ns1"><prefix:child1 xmlns:prefix="ns2">'+
              '<a0:child2 xmlns:a0="ns1"/></prefix:child1></prefix:root>');


  doc = ParseXML('<root xmlns="ns1"/>');
  root = doc.documentElement;
  child1 = doc.createElementNS("ns2", "child1");
  child2 = doc.createElementNS("ns1", "child2");
  child1.appendChild(child2);
  root.appendChild(child1);
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
                '<root xmlns="ns1"><child1 xmlns="ns2"><child2 xmlns="ns1"/>'+
                '</child1></root>');
}

function test7() {
  // Handle xmlns attribute declaring a default namespace on a non-namespaced
  // element (bug 326994).
  var doc = ParseXML('<root xmlns=""/>')
  var root = doc.documentElement;
  root.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns",
                      "http://www.w3.org/1999/xhtml");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc), '<root/>');

  doc = ParseXML('<root xmlns=""><child1/></root>')
  root = doc.documentElement;
  root.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns",
                      "http://www.w3.org/1999/xhtml");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc), '<root><child1/></root>');

  doc = ParseXML('<root xmlns="http://www.w3.org/1999/xhtml">' +
                 '<child1 xmlns=""><child2/></child1></root>')
  root = doc.documentElement;

  // No interface flattening in xpcshell
  var child1 = root.firstChild.QueryInterface(nsIDOMElement);
  child1.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns",
                        "http://www.w3.org/1999/xhtml");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="http://www.w3.org/1999/xhtml"><child1 xmlns="">' +
              '<child2/></child1></root>');

  doc = ParseXML('<root xmlns="http://www.w3.org/1999/xhtml">' +
                 '<child1 xmlns="">' +
                 '<child2 xmlns="http://www.w3.org/1999/xhtml"></child2>' +
                 '</child1></root>')
  root = doc.documentElement;
  // No interface flattening in xpcshell
  child1 = root.firstChild.QueryInterface(nsIDOMElement);
  var child2 = child1.firstChild.QueryInterface(nsIDOMElement);
  child1.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns",
                        "http://www.w3.org/1999/xhtml");
  child2.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns", "");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="http://www.w3.org/1999/xhtml"><child1 xmlns="">' +
              '<a0:child2 xmlns:a0="http://www.w3.org/1999/xhtml" xmlns=""></a0:child2></child1></root>');
}

function test8() {
  // Test behavior of serializing with a given charset.
  var str1 = '<?xml version="1.0" encoding="windows-1252"?>'+LB+'<root/>';
  var str2 = '<?xml version="1.0" encoding="UTF-8"?>'+LB+'<root/>';
  var doc1 = ParseXML(str1);
  var doc2 = ParseXML(str2);

  var p = Pipe();
  DOMSerializer().serializeToStream(doc1, p.outputStream, "windows-1252");
  p.outputStream.close();
  do_check_eq(ScriptableInput(p).read(-1), str1);

  p = Pipe();
  DOMSerializer().serializeToStream(doc2, p.outputStream, "windows-1252");
  p.outputStream.close();
  do_check_eq(ScriptableInput(p).read(-1), str1);

  p = Pipe();
  DOMSerializer().serializeToStream(doc1, p.outputStream, "UTF-8");
  p.outputStream.close();
  do_check_eq(ScriptableInput(p).read(-1), str2);

  p = Pipe();
  DOMSerializer().serializeToStream(doc2, p.outputStream, "UTF-8");
  p.outputStream.close();
  do_check_eq(ScriptableInput(p).read(-1), str2);
}

function test9() {
  // Test behavior of serializing between given charsets, using
  // windows-1252-representable text.
  var contents = '<root>' +
                   '\u00BD + \u00BE == \u00BD\u00B2 + \u00BC + \u00BE' +
                 '</root>';
  var str1 = '<?xml version="1.0" encoding="windows-1252"?>'+ LB + contents;
  var str2 = '<?xml version="1.0" encoding="UTF-8"?>'+ LB + contents;
  var str3 = '<?xml version="1.0" encoding="UTF-16"?>'+ LB + contents;
  var doc1 = ParseXML(str1);
  var doc2 = ParseXML(str2);
  var doc3 = ParseXML(str3);

  checkSerialization(doc1, "windows-1252", str1);
  checkSerialization(doc2, "windows-1252", str1);
  checkSerialization(doc3, "windows-1252", str1);

  checkSerialization(doc1, "UTF-8", str2);
  checkSerialization(doc2, "UTF-8", str2);
  checkSerialization(doc3, "UTF-8", str2);

  checkSerialization(doc1, "UTF-16", str2);
  checkSerialization(doc2, "UTF-16", str2);
  checkSerialization(doc3, "UTF-16", str2);
}

function test10() {
  // Test behavior of serializing between given charsets, using
  // Unicode characters (XXX but only BMP ones because I don't know
  // how to create one with non-BMP characters, either with JS strings
  // or using DOM APIs).
  var contents = '<root>' +
                   'AZaz09 \u007F ' +               // U+000000 to U+00007F
                   '\u0080 \u0398 \u03BB \u0725 ' + // U+000080 to U+0007FF
                   '\u0964 \u0F5F \u20AC \uFFFB' +  // U+000800 to U+00FFFF
                 '</root>';
  var str1 = '<?xml version="1.0" encoding="UTF-8"?>'+ LB + contents;
  var str2 = '<?xml version="1.0" encoding="UTF-16"?>'+ LB + contents;
  var doc1 = ParseXML(str1);
  var doc2 = ParseXML(str2);

  checkSerialization(doc1, "UTF8", str1);
  checkSerialization(doc2, "UTF8", str1);

  checkSerialization(doc1, "UTF-16", str1);
  checkSerialization(doc2, "UTF-16", str1);
}

function checkSerialization(doc, toCharset, expectedString) {
  var p = Pipe();
  DOMSerializer().serializeToStream(doc, p.outputStream, toCharset);
  p.outputStream.close();

  var inCharset = (toCharset == "UTF-16") ? "UTF-8" : toCharset;
  var cin = C["@mozilla.org/intl/converter-input-stream;1"]
             .createInstance(I.nsIConverterInputStream);
  cin.init(p.inputStream, inCharset, 1024, 0x0);

  // compare the first expectedString.length characters for equality
  var outString = {};
  var count = cin.readString(expectedString.length, outString);
  do_check_eq(count, expectedString.length);
  do_check_eq(outString.value, expectedString);

  // if there's anything more in the stream, it's a bug
  do_check_eq(0, cin.readString(1, outString));
  do_check_eq(outString.value, "");
}
