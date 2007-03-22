/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/

function traverse(node, indent)
{
  indent += "  ";
  var type = node.nodeType;

  // if it's an element dump the tag and recurse the children
  if (type == Node.ELEMENT_NODE) {
    dump(indent + "<" + node.tagName + ">\n");

    // go through the children
    if (node.hasChildNodes()) {
      var children = node.childNodes;
      var i, length = children.length;
      for (i = 0; i < length; i++) {
        var child = children[i];
        traverse(child, indent);
      }
      dump(indent + "</" + node.tagName + ">\n");
    }
  }
  // it's just text, no tag, dump "Text"
  else if (type == Node.TEXT_NODE) {
    dump(indent + node.data + "\n");
  }
}

function dumpTree()
{
  var node = document.documentElement;
  dump("Document Tree:\n");
  traverse(node, "");
  dump("\n");
}
