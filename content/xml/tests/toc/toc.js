
function toggleDisplay(event)
{
  var img = event.target;
  var div = img.nextSibling.nextSibling; 

  if (div.style.display == "none") {
    div.style.display = "block";
    img.src = "minus.gif";
  }
  else {
    div.style.display = "none";
    img.src = "plus.gif";
  }
}

var searchTags = new Array("book", "chapter", "section");
var tocTags = new Array("level1", "level2", "level3");
function addToToc(root, tocFrame)
{
  var i;
  var newTocFrame = tocFrame;
  var newTocElement = null;

  for (i=0; i < searchTags.length; i++) {
    if (root.tagName == searchTags[i]) {
      newTocElement = document.createElement(tocTags[i]);
      newTocElement.setAttribute("xml:link", "simple");
      newTocElement.setAttribute("href", "#"+ root.getAttribute("id"));
      newTocElement.setAttribute("show", "replace");

      if (i < searchTags.length-1) {
        // XXX DOM Level 1 does not have a way to specify the namespace
        // of an element at creation time. The W3C DOM Working Group is 
        // considering such a mechanism for DOM Level 2. In the interim, 
        // we've created the one used below. It will be removed in favor 
        //of the standard version in Level 2.
        var img = document.createElementWithNameSpace("img", "http://www.w3.org/TR/REC-html40");
        img.src = "minus.gif";
        img.onmouseup = toggleDisplay;
        newTocElement.appendChild(img);
 
        newTocFrame = document.createElementWithNameSpace("div", "http://www.w3.org/TR/REC-html40");
        newTocElement.appendChild(newTocFrame);
      }
      else {
        newTocFrame = null;
      }

      tocFrame.appendChild(newTocElement);

      break;
    }
  }

  for (i=0; i < root.childNodes.length; i++) {
    var child = root.childNodes[i];
    if (child.nodeType == Node.ELEMENT_NODE) {
      if ((newTocElement != null) && (child.tagName == "title")) {
        var text = child.firstChild.cloneNode(true);
        newTocElement.insertBefore(text, newTocFrame);
      }
      else {
        addToToc(child, newTocFrame);
      }
    }
  }
}


function createToc()
{
  if (document.getElementsByTagName("toc").length == 0) {
    var toc = document.createElement("toc");
    var title = document.createElement("title");
    title.appendChild(document.createTextNode("Table of Contents"));
    toc.appendChild(title);
  
    addToToc(document.documentElement, toc);
    document.styleSheets[0].insertRule("book {margin-left:12em;}", 0);
    document.documentElement.appendChild(toc);
  }
}

function initiateCreateToc()
{
   setTimeout(createToc, 0);
}

