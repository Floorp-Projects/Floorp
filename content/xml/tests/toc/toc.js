/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Event handler for display togglers in Table of Contents
function toggleDisplay(event)
{
  if (event.target.localName != "img")
    return;
  var img = event.target;
  var div = img.nextSibling.nextSibling; 

  // Change the display: property of the container to
  // hide and show the container.
  if (div.style.display == "none") {
    div.style.display = "block";
    img.src = "minus.gif";
  }
  else {
    div.style.display = "none";
    img.src = "plus.gif";
  }
}

// Function that recurses down the tree, looking for 
// structural elements. For each structural element,
// a corresponding element is created in the table of
// contents.
var searchTags = new Array("book", "chapter", "section");
var tocTags = new Array("level1", "level2", "level3");
function addToToc(root, tocFrame)
{
  var i;
  var newTocFrame = tocFrame;
  var newTocElement = null;
  var newTocLink = null;

  for (i=0; i < searchTags.length; i++) {
    if (root.tagName == searchTags[i]) {
      // If we've found a structural element, create the
      // equivalent TOC element.
      newTocElement = document.createElement(tocTags[i]);
      // Create the toclink element that is a link to the
      // corresponding structural element.
      newTocLink = document.createElement("toclink");
      newTocLink.setAttributeNS("http://www.w3.org/1999/xlink","xlink:type", "simple");
      newTocLink.setAttributeNS("http://www.w3.org/1999/xlink","xlink:href", "#"+ root.getAttribute("id"));
      newTocLink.setAttributeNS("http://www.w3.org/1999/xlink","xlink:show", "replace");
      newTocElement.appendChild(newTocLink);

      // Create the image and toggling container in the table of contents
      if (i < searchTags.length-1) {
        var img = document.createElementNS("http://www.w3.org/1999/xhtml","img");
        img.src = "minus.gif";
        newTocElement.insertBefore(img,newTocLink);
 
        newTocFrame = document.createElementNS("http://www.w3.org/1999/xhtml","div");
        newTocElement.appendChild(newTocFrame);
      }
      else {
        newTocFrame = null;
      }

      tocFrame.appendChild(newTocElement);

      break;
    }
  }

  // Recurse down through the childNodes list
  for (i=0; i < root.childNodes.length; i++) {
    var child = root.childNodes[i];
    if (child.nodeType == Node.ELEMENT_NODE) {
      if ((newTocLink != null) && (child.tagName == "title")) {
        var text = child.firstChild.cloneNode(true);
        newTocLink.appendChild(text);
      }
      else {
        addToToc(child, newTocFrame);
      }
    }
  }
}

// Create the root table of contents element (a fixed element)
// and its contents.
function createToc()
{
  if (document.getElementsByTagName("toc").length == 0) {
    var toc = document.createElement("toc");
    var title = document.createElement("title");
    title.appendChild(document.createTextNode("Table of Contents"));
    toc.appendChild(title);
  
    // Recurse down and build up the document element
    addToToc(document.documentElement, toc);
    
    // Since we've created the toc element as a fixed element,
    // insert a rule that shifts over the document element by
    // the width of the toc element.
    document.styleSheets[0].cssRules[0].style.marginLeft = "12em";
    document.documentElement.appendChild(toc);
    
    // Attach the event handler for table of contents buttons.
    // This will only work for content that is already a part
    // of a document, which is why we had to wait until here
    // to do this.
    toc.addEventListener("mouseup",toggleDisplay,1);
  } else {
    // Hide the table of contents.
    // This is not very intelligent if we have a static document, we should
    // just hide/show the toc via stylesheet mungling
    document.documentElement.removeChild(document.getElementsByTagName("toc")[0]);
    document.styleSheets[0].cssRules[0].style.marginLeft = "0em";
  }
}

