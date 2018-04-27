/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
function run_test()
{
  
  test_getElementsByTagName();
  test_getElementsByTagNameNS();
  test_getElementsByAttribute();
  test_getElementsByAttributeNS();

  // What else should we test?
  // XXXbz we need more tests here to test liveness!

}  

function test_getElementsByTagName()
{
  var doc = ParseFile("nodelist_data_1.xml");
  var root = doc.documentElement;

  // Check that getElementsByTagName returns an HTMLCollection.
  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagName("*")),
               "HTMLCollection")
  Assert.ok(ChromeUtils.getClassName(root.getElementsByTagName("*")),
            "HTMLCollection");
  
  // Check that getElementsByTagName excludes the element it's called on.
  Assert.equal(doc.getElementsByTagName("*").length,
               root.getElementsByTagName("*").length + 1);
  Assert.equal(doc.getElementById("test2").getElementsByTagName("*").length,
               8);
  Assert.equal(doc.getElementById("test2").getElementsByTagName("test").length,
               3);

  // Check that the first element of getElementsByTagName on the document is
  // the right thing.
  Assert.equal(doc.getElementsByTagName("*").item(0), root);

  // Check that we get the right things in the right order
  var numTests = doc.getElementsByTagName("test").length;
  Assert.equal(numTests, 5);

  for (var i = 1; i <= numTests; ++i) {
    Assert.ok(Element.isInstance(doc.getElementById("test" + i)));
    Assert.equal(doc.getElementById("test" + i),
                 doc.getElementsByTagName("test").item(i-1));
  }

  // Check that we handle tagnames containing ':' correctly
  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagName("foo:test")),
               "HTMLCollection");
  Assert.equal(doc.getElementsByTagName("foo:test").length, 2);

  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagName("foo2:test")),
               "HTMLCollection");
  Assert.equal(doc.getElementsByTagName("foo2:test").length, 3);

  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagName("bar:test")),
               "HTMLCollection");
  Assert.equal(doc.getElementsByTagName("bar:test").length, 4);
}

function test_getElementsByTagNameNS()
{
  var doc = ParseFile("nodelist_data_1.xml");
  var root = doc.documentElement;

  // Check that getElementsByTagNameNS returns an HTMLCollection.
  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagNameNS("*", "*")),
               "HTMLCollection");
  Assert.equal(ChromeUtils.getClassName(root.getElementsByTagNameNS("*", "*")),
               "HTMLCollection");

  // Check that passing "" and null for the namespace URI gives the same result
  var list1 = doc.getElementsByTagNameNS("", "test");
  var list2 = doc.getElementsByTagNameNS(null, "test");
  Assert.equal(list1.length, list2.length);
  for (var i = 0; i < list1.length; ++i) {
    Assert.equal(list1.item(i), list2.item(i));
  }
  
  // Check that getElementsByTagNameNS excludes the element it's called on.
  Assert.equal(doc.getElementsByTagNameNS("*", "*").length,
               root.getElementsByTagNameNS("*", "*").length + 1);
  Assert.equal(doc.getElementById("test2")
                  .getElementsByTagNameNS("*", "*").length,
               8);
  Assert.equal(doc.getElementById("test2")
                  .getElementsByTagNameNS("", "test").length,
               1);
  Assert.equal(doc.getElementById("test2")
                  .getElementsByTagNameNS("*", "test").length,
               7);

  // Check that the first element of getElementsByTagNameNS on the document is
  // the right thing.
  Assert.equal(doc.getElementsByTagNameNS("*", "*").item(0), root);
  Assert.equal(doc.getElementsByTagNameNS(null, "*").item(0), root);

  // Check that we get the right things in the right order

     
  var numTests = doc.getElementsByTagNameNS("*", "test").length;
  Assert.equal(numTests, 14);

  for (var i = 1; i <= numTests; ++i) {
    Assert.ok(Element.isInstance(doc.getElementById("test" + i)));
    Assert.equal(doc.getElementById("test" + i),
                 doc.getElementsByTagNameNS("*", "test").item(i-1));
  }

  // Check general proper functioning of having a non-wildcard namespace.
  var test2 = doc.getElementById("test2");
  Assert.equal(doc.getElementsByTagNameNS("", "test").length,
               3);
  Assert.equal(test2.getElementsByTagNameNS("", "test").length,
               1);
  Assert.equal(doc.getElementsByTagNameNS("foo", "test").length,
               7);
  Assert.equal(test2.getElementsByTagNameNS("foo", "test").length,
               4);
  Assert.equal(doc.getElementsByTagNameNS("foo2", "test").length,
               0);
  Assert.equal(test2.getElementsByTagNameNS("foo2", "test").length,
               0);
  Assert.equal(doc.getElementsByTagNameNS("bar", "test").length,
               4);
  Assert.equal(test2.getElementsByTagNameNS("bar", "test").length,
               2);

  // Check that we handle tagnames containing ':' correctly
  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagNameNS(null, "foo:test")),
               "HTMLCollection");
  Assert.equal(doc.getElementsByTagNameNS(null, "foo:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("foo", "foo:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("bar", "foo:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("*", "foo:test").length, 0);
  
  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagNameNS(null, "foo2:test")),
               "HTMLCollection");
  Assert.equal(doc.getElementsByTagNameNS(null, "foo2:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("foo2", "foo2:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("bar", "foo2:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("*", "foo2:test").length, 0);
  
  Assert.equal(ChromeUtils.getClassName(doc.getElementsByTagNameNS(null, "bar:test")),
               "HTMLCollection");
  Assert.equal(doc.getElementsByTagNameNS(null, "bar:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("bar", "bar:test").length, 0);
  Assert.equal(doc.getElementsByTagNameNS("*", "bar:test").length, 0);

  // Check that previously-unknown namespaces are handled right.  Note that we
  // can just hardcode the strings, since we're running only once in XPCshell.
  // If someone wants to run these in a browser, some use of Math.random() may
  // be in order.
  list1 = doc.getElementsByTagNameNS("random-bogus-namespace", "foo");
  list2 = doc.documentElement.getElementsByTagNameNS("random-bogus-namespace2",
                                                     "foo");
  Assert.notEqual(list1, list2);
  Assert.equal(list1.length, 0);
  Assert.equal(list2.length, 0);
  var newNode = doc.createElementNS("random-bogus-namespace", "foo");
  doc.documentElement.appendChild(newNode);
  var newNode = doc.createElementNS("random-bogus-namespace2", "foo");
  doc.documentElement.appendChild(newNode);
  Assert.equal(list1.length, 1);  
  Assert.equal(list2.length, 1);  
}

function test_getElementsByAttribute()
{
  var doc = ParseFile("nodelist_data_2.xul");
  var root = doc.documentElement;

  // Sadly, DOMParser can't create XULDocument objects.  But at least we have a
  // XULElement!

  Assert.equal(ChromeUtils.getClassName(root), "XULElement");

  Assert.equal(ChromeUtils.getClassName(root.getElementsByAttribute("foo", "foo")),
               "HTMLCollection");

  var master1 = doc.getElementById("master1");
  var master2 = doc.getElementById("master2");
  var master3 = doc.getElementById("master3");
  var external = doc.getElementById("external");

  Assert.equal(ChromeUtils.getClassName(master1), "XULElement");
  Assert.equal(ChromeUtils.getClassName(master2), "XULElement");
  Assert.equal(ChromeUtils.getClassName(master3), "XULElement");
  Assert.equal(ChromeUtils.getClassName(external), "XULElement");

  // Basic tests
  Assert.equal(root.getElementsByAttribute("foo", "foo").length,
               14);
  Assert.equal(master1.getElementsByAttribute("foo", "foo").length,
               4);

  Assert.equal(root.getElementsByAttribute("foo", "bar").length,
               7);
  Assert.equal(master1.getElementsByAttribute("foo", "bar").length,
               2);

  Assert.equal(root.getElementsByAttribute("bar", "bar").length,
               7);
  Assert.equal(master1.getElementsByAttribute("bar", "bar").length,
               2);

  Assert.equal(root.getElementsByAttribute("foo", "*").length,
               21);
  Assert.equal(master1.getElementsByAttribute("foo", "*").length,
               6);

  // Test the various combinations of attributes with colons in the name
  Assert.equal(root.getElementsByAttribute("foo:foo", "foo").length,
               16);
  Assert.equal(master1.getElementsByAttribute("foo:foo", "foo").length,
               5);
  Assert.equal(master2.getElementsByAttribute("foo:foo", "foo").length,
               4);
  Assert.equal(master3.getElementsByAttribute("foo:foo", "foo").length,
               4);
  Assert.equal(external.getElementsByAttribute("foo:foo", "foo").length,
               2);

  Assert.equal(root.getElementsByAttribute("foo:foo", "bar").length,
               9);
  Assert.equal(master1.getElementsByAttribute("foo:foo", "bar").length,
               2);
  Assert.equal(master2.getElementsByAttribute("foo:foo", "bar").length,
               3);
  Assert.equal(master3.getElementsByAttribute("foo:foo", "bar").length,
               2);
  Assert.equal(external.getElementsByAttribute("foo:foo", "bar").length,
               1);

  Assert.equal(root.getElementsByAttribute("foo:bar", "foo").length,
               7);
  Assert.equal(master1.getElementsByAttribute("foo:bar", "foo").length,
               2);
  Assert.equal(master2.getElementsByAttribute("foo:bar", "foo").length,
               2);
  Assert.equal(master3.getElementsByAttribute("foo:bar", "foo").length,
               2);
  Assert.equal(external.getElementsByAttribute("foo:bar", "foo").length,
               1);

  Assert.equal(root.getElementsByAttribute("foo:bar", "bar").length,
               14);
  Assert.equal(master1.getElementsByAttribute("foo:bar", "bar").length,
               4);
  Assert.equal(master2.getElementsByAttribute("foo:bar", "bar").length,
               4);
  Assert.equal(master3.getElementsByAttribute("foo:bar", "bar").length,
               4);
  Assert.equal(external.getElementsByAttribute("foo:bar", "bar").length,
               2);

  Assert.equal(root.getElementsByAttribute("foo2:foo", "foo").length,
               8);
  Assert.equal(master1.getElementsByAttribute("foo2:foo", "foo").length,
               2);
  Assert.equal(master2.getElementsByAttribute("foo2:foo", "foo").length,
               2);
  Assert.equal(master3.getElementsByAttribute("foo2:foo", "foo").length,
               3);
  Assert.equal(external.getElementsByAttribute("foo2:foo", "foo").length,
               1);

  Assert.equal(root.getElementsByAttribute("foo:foo", "*").length,
               25);
  Assert.equal(master1.getElementsByAttribute("foo:foo", "*").length,
               7);
  Assert.equal(master2.getElementsByAttribute("foo:foo", "*").length,
               7);
  Assert.equal(master3.getElementsByAttribute("foo:foo", "*").length,
               6);
  Assert.equal(external.getElementsByAttribute("foo:foo", "*").length,
               3);

  Assert.equal(root.getElementsByAttribute("foo2:foo", "bar").length,
               0);
  Assert.equal(root.getElementsByAttribute("foo:foo", "baz").length,
               0);
}

function test_getElementsByAttributeNS()
{
  var doc = ParseFile("nodelist_data_2.xul");
  var root = doc.documentElement;

  // Sadly, DOMParser can't create XULDocument objects.  But at least we have a
  // XULElement!

  Assert.equal(ChromeUtils.getClassName(root), "XULElement");

  // Check that getElementsByAttributeNS returns an HTMLCollection.
  Assert.equal(ChromeUtils.getClassName(root.getElementsByAttributeNS("*", "*", "*")),
               "HTMLCollection");

  var master1 = doc.getElementById("master1");
  var master2 = doc.getElementById("master2");
  var master3 = doc.getElementById("master3");
  var external = doc.getElementById("external");

  Assert.equal(ChromeUtils.getClassName(master1), "XULElement");
  Assert.equal(ChromeUtils.getClassName(master2), "XULElement");
  Assert.equal(ChromeUtils.getClassName(master3), "XULElement");
  Assert.equal(ChromeUtils.getClassName(external), "XULElement");
  
  // Test wildcard namespace
  Assert.equal(root.getElementsByAttributeNS("*", "foo", "foo").length,
               38);
  Assert.equal(master1.getElementsByAttributeNS("*", "foo", "foo").length,
               11);
  Assert.equal(master2.getElementsByAttributeNS("*", "foo", "foo").length,
               10);
  Assert.equal(master3.getElementsByAttributeNS("*", "foo", "foo").length,
               11);

  Assert.equal(root.getElementsByAttributeNS("*", "foo", "bar").length,
               16);
  Assert.equal(master1.getElementsByAttributeNS("*", "foo", "bar").length,
               4);
  Assert.equal(master2.getElementsByAttributeNS("*", "foo", "bar").length,
               5);
  Assert.equal(master3.getElementsByAttributeNS("*", "foo", "bar").length,
               4);

  Assert.equal(root.getElementsByAttributeNS("*", "bar", "bar").length,
               21);
  Assert.equal(master1.getElementsByAttributeNS("*", "bar", "bar").length,
               6);
  Assert.equal(master2.getElementsByAttributeNS("*", "bar", "bar").length,
               6);
  Assert.equal(master3.getElementsByAttributeNS("*", "bar", "bar").length,
               6);

  Assert.equal(root.getElementsByAttributeNS("*", "foo", "*").length,
               54);
  Assert.equal(master1.getElementsByAttributeNS("*", "foo", "*").length,
               15);
  Assert.equal(master2.getElementsByAttributeNS("*", "foo", "*").length,
               15);
  Assert.equal(master3.getElementsByAttributeNS("*", "foo", "*").length,
               15);

  // Test null namespace. This should be the same as getElementsByAttribute.
  Assert.equal(root.getElementsByAttributeNS("", "foo", "foo").length,
               root.getElementsByAttribute("foo", "foo").length);
  Assert.equal(master1.getElementsByAttributeNS("", "foo", "foo").length,
               master1.getElementsByAttribute("foo", "foo").length);
  Assert.equal(master2.getElementsByAttributeNS("", "foo", "foo").length,
               master2.getElementsByAttribute("foo", "foo").length);
  Assert.equal(master3.getElementsByAttributeNS("", "foo", "foo").length,
               master3.getElementsByAttribute("foo", "foo").length);
  
  // Test namespace "foo"
  Assert.equal(root.getElementsByAttributeNS("foo", "foo", "foo").length,
               24);
  Assert.equal(master1.getElementsByAttributeNS("foo", "foo", "foo").length,
               7);
  Assert.equal(master2.getElementsByAttributeNS("foo", "foo", "foo").length,
               6);
  Assert.equal(master3.getElementsByAttributeNS("foo", "foo", "foo").length,
               7);

  Assert.equal(root.getElementsByAttributeNS("foo", "foo", "bar").length,
               9);
  Assert.equal(master1.getElementsByAttributeNS("foo", "foo", "bar").length,
               2);
  Assert.equal(master2.getElementsByAttributeNS("foo", "foo", "bar").length,
               3);
  Assert.equal(master3.getElementsByAttributeNS("foo", "foo", "bar").length,
               2);

  Assert.equal(root.getElementsByAttributeNS("foo", "bar", "foo").length,
               7);
  Assert.equal(master1.getElementsByAttributeNS("foo", "bar", "foo").length,
               2);
  Assert.equal(master2.getElementsByAttributeNS("foo", "bar", "foo").length,
               2);
  Assert.equal(master3.getElementsByAttributeNS("foo", "bar", "foo").length,
               2);

  Assert.equal(root.getElementsByAttributeNS("foo", "bar", "bar").length,
               14);
  Assert.equal(master1.getElementsByAttributeNS("foo", "bar", "bar").length,
               4);
  Assert.equal(master2.getElementsByAttributeNS("foo", "bar", "bar").length,
               4);
  Assert.equal(master3.getElementsByAttributeNS("foo", "bar", "bar").length,
               4);
}
