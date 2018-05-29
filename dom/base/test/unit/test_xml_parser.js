function run_test () {
  for (var i = 0; i < tests.length && tests[i][0]; ++i) {
    if (!tests[i][0].call()) {
      do_throw(tests[i][1]);
    }
  }
}

var tests = [
  [ test1, "Unable to parse basic XML document" ],
  [ test2, "ParseXML doesn't return Document" ],
  [ test3, "ParseXML return value's documentElement is not Element" ],
  [ test4, "" ],
  [ test5, "" ],
  [ test6, "" ],
  [ null ]
];

function test1() {
  return ParseXML("<root/>");
}

function test2() {
  return (ChromeUtils.getClassName(ParseXML("<root/>")) === "XMLDocument");
}

function test3() {
  return Element.isInstance(ParseXML("<root/>").documentElement);
}

function test4() {
  var doc = ParseXML("<root/>");
  Assert.equal(doc.documentElement.namespaceURI, null); 
  return true;
}

function test5() {
  var doc = ParseXML("<root xmlns=''/>");
  Assert.equal(doc.documentElement.namespaceURI, null); 
  return true;
}

function test6() {
  var doc = ParseXML("<root xmlns='ns1'/>");
  Assert.notEqual(doc.documentElement.namespaceURI, null); 
  Assert.equal(doc.documentElement.namespaceURI, 'ns1'); 
  return true;
}
