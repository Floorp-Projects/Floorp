
// An inefficient, but effective bubble sort
function sort(collection, key)
{
  var i, j;
  var count = collection.length;
  var parent, child;
 
  for (i = count-1; i >= 0; i--) {
    for (j = 1; j <= i; j++) {
      if (collection[j-1][key] > collection[j][key]) {
         // Move the item both in the local array and
         // in the tree
         child = collection[j];
         parent = child.parentNode;

         collection[j] = collection[j-1];
         collection[j-1] = child;

         parent.removeChild(child);       
         parent.insertBefore(child, collection[j]);
      }
    }
  }
}

// Set user properties on the nodes in the collection
// based on information found in its children. For example,
// make a property "Author" based on the content of the
// "Author" element found in the childNode list of the node.
// This makes later sorting more efficient
function collectInfo(nodes, propNames)
{
  var i, j, k;
  var ncount = nodes.length; 
  var pcount = propNames.length;

  for (i = 0; i < ncount; i++) {
    var node = nodes[i];
    var childNodes = node.childNodes;
    var ccount = childNodes.length;
 
    for (j = 0; j < ccount; j++) {
      var child = childNodes[j];

      if (child.nodeType == Node.ELEMENT_NODE) {
        var tagName = child.tagName;

        for (k = 0; k < pcount; k++) {
          var prop = propNames[k];
          if (prop == tagName) {
            node[prop] = child.firstChild.data;
          }  
        }
      }    
    }
  }
}

var enabled = true;
function toggleStyleSheet()
{
  if (enabled) {
    document.styleSheets[2].disabled = true;
  }
  else {
    document.styleSheets[2].disabled = false;
  }

  enabled = !enabled;
}

// XXX This is a workaround for a bug where
// changing the disabled state of a stylesheet can't
// be done in an event handler. For now, we do it
// in a zero-delay timeout.
function initiateToggle()
{
  setTimeout(toggleStyleSheet, 0);
}

var sortableProps = new Array("Author", "Title", "ISBN");
var books = new Array();

// We uppercase the tagName as a workaround for a bug
// that loses the original case of the tag.
var bookset = document.getElementsByTagName("Book");

// We need to create a "non-live" array to operate on. Since
// we'll be moving things around in this array, we can't use
// the read-only, live one returned by getElementsByTagName.
for (var i=0; i < bookset.length; i++) {
  books[i] = bookset[i];
}

collectInfo(books, sortableProps);

