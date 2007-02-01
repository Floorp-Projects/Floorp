/** Test for Bug 357450 **/

SimpleTest.waitForExplicitFinish();

function testGetElements (root, classtestCount) {

  ok(root.getElementsByClassName, "getElementsByClassName exists");
  is(typeof(root.getElementsByClassName), "function", "getElementsByClassName is a function");

  var nodes = root.getElementsByClassName("f");

  is(typeof(nodes.item), "function");
  is(typeof(nodes.length), "number");
  is(nodes.length, 0, "string with no matching class should get an empty list");

  nodes = root.getElementsByClassName("foo");
  ok(nodes, "should have nodelist object");

  // HTML5 says ints are allowed in class names
  // should match int class

  nodes = root.getElementsByClassName("1");
  is(nodes[0], $('int-class'), "match integer class name");
  nodes = root.getElementsByClassName([1]);
  is(nodes[0], $('int-class'), "match integer class name 2");
  nodes = root.getElementsByClassName(["1 junk"]);

  is(nodes.length, 0, "two classes, but no elements have both");

  nodes = root.getElementsByClassName("test1");
  is(nodes[0], $('test1'), "Id and class name turn up the same node");
  nodes = root.getElementsByClassName("test1 test2");
  is(nodes.length, 0, "two classes, but no elements have both");

  // WHATWG examples
  nodes = document.getElementById('example').getElementsByClassName('aaa');
  is(nodes.length, 2, "returns 2 elements");

  nodes = document.getElementById('example').getElementsByClassName('ccc bbb') 
  is(nodes.length, 1, "only match elements that have all the classes specified in that array. tokenize string arg.")
  is(nodes[0], $('p3'), "matched tokenized string");

  nodes = document.getElementById('example').getElementsByClassName('');
  is(nodes.length, 0, "class name with empty string shouldn't return nodes");

  nodes = root.getElementsByClassName({});
  ok(nodes, "bogus arg shouldn't be null");
  is(typeof(nodes.item), "function");
  is(typeof(nodes.length), "number");
  is(nodes.length, 0, "bogus arg should get an empty nodelist");
}

addLoadEvent(function() {
  if (document.getElementsByName) {
    var anchorNodes = document.getElementsByName("nametest");
    is(anchorNodes.length, 1, "getElementsByName still works");
    is(anchorNodes[0].getAttribute("name"), "nametest", 
       "getElementsByName still works");
  }
  //testGetElements($("content"), 1);
  //testGetElements(document.documentElement, 3);
  testGetElements(document, 3); 
});
addLoadEvent(SimpleTest.finish);
