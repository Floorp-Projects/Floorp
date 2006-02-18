function run_test() {
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
  node.setAttributeNS("", "p3:l3", "v3");
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
  var child1 = root.firstChild;
  child1.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns",
                        "http://www.w3.org/1999/xhtml");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="http://www.w3.org/1999/xhtml"><child1 xmlns="">' +
              '<child2/></child1></root>');

  doc = ParseXML('<root xmlns="http://www.w3.org/1999/xhtml">' +
                 '<child1 xmlns="">' +
                 '<child2 xmlns="http://www.w3.org/1999/xhtml"/>' +
                 '</child1></root>')
  root = doc.documentElement;
  child1 = root.firstChild;
  var child2 = child1.firstChild;
  child1.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns",
                        "http://www.w3.org/1999/xhtml");
  child2.setAttributeNS("http://www.w3.org/2000/xmlns/", "xmlns", "");
  do_check_serialize(doc);
  do_check_eq(SerializeXML(doc),
              '<root xmlns="http://www.w3.org/1999/xhtml"><child1 xmlns="">' +
              '<a0:child2 xmlns:a0="http://www.w3.org/1999/xhtml" xmlns=""/></child1></root>');
}
