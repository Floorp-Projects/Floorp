/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Ben Goodger
 */

/*
  Script for the file properties window
*/

function FileProperties()
{
  var tree = document.getElementById('fileTree');

  if (tree.selectedItems.length >= 1) {
  // don't bother showing properties on bookmark separators
  var type = tree.selectedItems[0].getAttribute('type');
    if (type != "http://home.netscape.com/NC-rdf#BookmarkSeparator") {
    var props = window.open("chrome://communicator/content/bookmarks/bm-props.xul",
                                "BookmarkProperties", "chrome,menubar,resizable");
    props.BookmarkURL = tree.selectedItems[0].getAttribute("id");
  }
  } else {
    dump("nothing selected!\n");
  }
}

function OpenSearch(tabName)
{
  window.openDialog("resource:/res/samples/search.xul", "SearchWindow", "dialog=no,close,chrome,resizable", tabName);
}

function OpenURL(event, node)
{
    // clear any single-click/edit timeouts
    if (timerID != null)
    {
        gEditNode = null;
        clearTimeout(timerID);
        timerID = null;
    }

    if (node.getAttribute('container') == "true")
    {
        return(false);
    }

    var url = node.getAttribute('id');

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:")
    {
        return(false);
    }

  try
  {
    // add support for IE favorites under Win32, and NetPositive URLs under BeOS
    if (url.indexOf("file://") == 0)
    {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
      if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
      if (rdf)
      {
        var fileSys = rdf.GetDataSource("rdf:files");
        if (fileSys)
        {
          var src = rdf.GetResource(url, true);
          var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
          var target = fileSys.GetTarget(src, prop, true);
          if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (target) target = target.Value;
          if (target) url = target;

        }
      }
    }
  }
  catch(ex)
  {
  }

    if(top.isEditor != undefined) {
        if(top.editorShell) {
            var ext = getFileExtension(url);
            // look at the extension of the file to see what should be done
            // with it. Note that this is a typically windowsey approach. will
            // rdf:files hold a reference to the actual file type? (Mac?)
            switch(ext) {
            // XXX Crude, but it will do for now.
            case "gif":
            case "jpeg":
            case "jpg":
            case "png":
                // just insert the image
                EditorAutoInsertImage(url);
                break;
            case "htm":
            case "html":
            case "xul":
                toEditor(url);
                break;
            case "js":
                EditorInsertJSFile(url);
                break;
            case "css":
                EditorInsertCSSFile(url);
                break;
            default:
                // load the file in the editor
                dump("Editor Message: Weirdo File Format\n");
                toEditor(url);
                break;
            }
        }
    } else {
        window.open(url,'bookmarks');
    }

    dump("OpenURL(" + url + ")\n");

    return(true);
}

// returns the extension of a specified file URL
function getFileExtension(url)
{
    return url.substring(url.lastIndexOf(".")+1,url.length).toLowerCase();
}

var htmlInput = null;
var saveNode = null;
var newValue = "";
var timerID = null;
var gEditNode = null;

function DoSingleClick(event, node)
{
  var type = node.parentNode.parentNode.getAttribute('type');
  var selected = node.parentNode.parentNode.getAttribute('selected');

  if (gEditNode == node) {
    // Only start an inline edit if it is the second consecutive click
    // on the same node that is not already editing or a separator.
    if (!htmlInput &&
        type != "http://home.netscape.com/NC-rdf#BookmarkSeparator") {
      // Edit node if we don't get a double-click in less than 1/2 second
      timerID = setTimeout("OpenEditNode()", 500);
    }
  } else {
    if (htmlInput) {
      // Clicked during an edit
      // Save the changes and move on
      CloseEditNode(true);
    }
    gEditNode = node;
  }
  return false;
}

function OpenEditNode()
{
    dump("OpenEditNode entered.\n");

    // clear any single-click/edit timeouts
    if (timerID != null)
    {
        clearTimeout(timerID);
        timerID = null;
    }

    // XXX uncomment the following line to replace the whole input row we do this
    // (and, therefore, only allow editing on the name column) until we can
    // arbitrarily change the content model (bugs prevent this at the moment)
    gEditNode = gEditNode.parentNode;

    var name = gEditNode.parentNode.getAttribute("Name");
    dump("Single click on '" + name + "'\n");

    var theParent = gEditNode.parentNode;
    dump("Parent node is a " + theParent.nodeName + "\n\n");

    saveNode = gEditNode;

    // unselect all nodes!
    var select_list = document.getElementsByAttribute("selected", "true");
    dump("# of Nodes selected: " + select_list.length + "\n\n");
    for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
    {
        var node = select_list[nodeIndex];
        if (node)
        {
          dump("Unselecting node "+node.getAttribute("Name") + "\n");
          node.removeAttribute("selected");
        }
    }

    // XXX for now, just remove the child from the parent
    // optimally, we'd like to not remove the child-parent relationship
    // and instead just set a "display: none;" attribute on the child node

//    gEditNode.setAttribute("style", "display: none;");
//    dump("gEditNode hidden.\n");
    theParent.removeChild(gEditNode);
    gEditNode = null;
    dump("gEditNode removed.\n");

    // create the html:input node
    htmlInput = document.createElementNS("http://www.w3.org/1999/xhtml", "html:input");
    htmlInput.setAttribute("value", name);
    htmlInput.setAttribute("onkeypress", "return EditNodeKeyPress(event)");

    theParent.appendChild(htmlInput);
    dump("html:input node added.\n");

    htmlInput.focus();

    dump("OpenEditNode done.\n");
    return(true);
}

function CloseEditNode(saveChangeFlag)
{
    dump("CloseEditNode entered.\n");

    if (htmlInput)
    {
        if (saveChangeFlag)
        {
            newValue = htmlInput.value;
        }
        dump("  Got html input: "+newValue+" \n");

        var theParent = htmlInput.parentNode;
        theParent.removeChild(htmlInput);
        theParent.appendChild(saveNode);
        dump("  child node appended.\n");

        if (saveNode && saveChangeFlag)
        {
            var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
            RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
            var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
            dump("Got bookmarks datasource.\n");

            // XXX once we support multi-column editing, get the property
            // from the column in the content model
            var propertyName = "http://home.netscape.com/NC-rdf#Name";
            var propertyNode = RDF.GetResource(propertyName, true);
            dump("  replacing value of property '" + propertyName + "'\n");

            // get the URI
            var theNode = saveNode;
            var bookmarkURL = "";
            while(true)
            {
                var tag = theNode.nodeName;
                if (tag == "treeitem")
                {
                    bookmarkURL = theNode.getAttribute("id");
                    break;
                }
                theNode = theNode.parentNode;
            }
            dump("  uri is '" + bookmarkURL + "'\n");

            if (bookmarkURL == "")    return(false);
            var bookmarkNode = RDF.GetResource(bookmarkURL, true);


            dump("  newValue = '" + newValue + "'\n");
            newValue = (newValue != "") ? RDF.GetLiteral(newValue) : null;

            var oldValue = Bookmarks.GetTarget(bookmarkNode, propertyNode, true);
            if (oldValue)
            {
                oldValue = oldValue.QueryInterface(Components.interfaces.nsIRDFLiteral);
                dump("  oldValue = '" + oldValue + "'\n");
            }

            if (oldValue != newValue)
            {
                if (oldValue && !newValue)
                {
                    Bookmarks.Unassert(bookmarkNode, propertyNode, oldValue);
                    dump("  Unassert used.\n");
                }
                else if (!oldValue && newValue)
                {
                    Bookmarks.Assert(bookmarkNode, propertyNode, newValue, true);
                    dump("  Assert used.\n");
                }
                else if (oldValue && newValue)
                {
                    Bookmarks.Change(bookmarkNode, propertyNode, oldValue, newValue);
                    dump("  Change used.\n");
                }

                dump("re-writing bookmarks.html\n");
                var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
                remote.Flush();
            }

            newValue = "";
            saveNode = null;
        }
        else dump("saveNode was null?\n");
        htmlInput = null;
    }
    else dump("htmlInput was null?\n");

    dump("CloseEditNode done.\n");
}

function EditNodeKeyPress(event)
{
    if (event.keyCode == 27)
    {
        CloseEditNode(false);
        return(false);
    }
    else if (event.keyCode == 13 || event.keyCode == 10)
    {
        CloseEditNode(true);
        return(false);
    }
    return(true);
}

function doSort(sortColName)
{
  var node = document.getElementById(sortColName);
  // determine column resource to sort on
  var sortResource = node.getAttribute('resource');
  if (!node) return(false);

  var sortDirection="ascending";
  var isSortActive = node.getAttribute('sortActive');
  if (isSortActive == "true") {
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
      sortDirection = "descending";
    else if (currentDirection == "descending")
      sortDirection = "natural";
    else
      sortDirection = "ascending";
  }

  // get RDF Core service
  var rdfCore = XPAppCoresManager.Find("RDFCore");
  if (!rdfCore) {
    rdfCore = new RDFCore();
    if (!rdfCore) {
      return(false);
    }
    rdfCore.Init("RDFCore");
//    XPAppCoresManager.Add(rdfCore);
  }
  // sort!!!
  rdfCore.doSort(node, sortResource, sortDirection);
  return(false);
}


function fillContextMenu(name,node)
{
    if (!name)    return(false);
    var popupNode = document.getElementById(name);
    if (!popupNode)    return(false);

    var url = GetFileURL(node);
    var ext = getFileExtension(url);

    // remove the menu node (which tosses all of its kids);
    // do this in case any old command nodes are hanging around
    var menuNode = popupNode.childNodes[0];
    popupNode.removeChild(menuNode);

    // create a new menu node
    menuNode = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "menu");
    popupNode.appendChild(menuNode);

    dump("mwa");
    if(ext == "gif")
    {
        // note: deleted all the doContextCmd stuff from bookmarks.
        menuItem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "menuitem");
        menuItem.setAttribute("label","Insert Image");
        // menuItem.setAttribute("onaction","AutoInsertImage(\'" + url + "\')");
        parent.appendChild(menuItem);
    }

    return(true);
}

function isImageFile(parent,url)
{
}

function GetFileURL(node)
{
    var url = node.getAttribute('id');

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:")
    {
        return(false);
    }

  try
  {
    // add support for IE favorites under Win32, and NetPositive URLs under BeOS
    if (url.indexOf("file://") == 0)
    {
      var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
      if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
      if (rdf)
      {
        var fileSys = rdf.GetDataSource("rdf:files");
        if (fileSys)
        {
          var src = rdf.GetResource(url, true);
          var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
          var target = fileSys.GetTarget(src, prop, true);
          if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (target) target = target.Value;
          if (target) url = target;

        }
      }
    }
  }
  catch(ex)
  {
  }
    return url;
}



function doContextCmd(cmdName)
{
  dump("doContextCmd start: cmd='" + cmdName + "'\n");

  var treeNode = document.getElementById("bookmarksTree");
  if (!treeNode)    return(false);
  var db = treeNode.database;
  if (!db)    return(false);

  var compositeDB = db.QueryInterface(Components.interfaces.nsIRDFDataSource);
  if (!compositeDB)    return(false);

  var isupports = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
  if (!isupports)    return(false);
  var rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
  if (!rdf)    return(false);

  // need a resource for the command
  var cmdResource = rdf.GetResource(cmdName);
  if (!cmdResource)        return(false);
  cmdResource = cmdResource.QueryInterface(Components.interfaces.nsIRDFResource);
  if (!cmdResource)        return(false);

  var select_list = treeNode.getElementsByAttribute("selected", "true");
  if (select_list.length < 1)    return(false);

  dump("# of Nodes selected: " + select_list.length + "\n\n");

  // set up selection nsISupportsArray
  var selectionInstance = Components.classes["@mozilla.org/supports-array;1"].createInstance();
  var selectionArray = selectionInstance.QueryInterface(Components.interfaces.nsISupportsArray);

  // set up arguments nsISupportsArray
  var argumentsInstance = Components.classes["@mozilla.org/supports-array;1"].createInstance();
  var argumentsArray = argumentsInstance.QueryInterface(Components.interfaces.nsISupportsArray);

  // get argument (parent)
  var parentArc = rdf.GetResource("http://home.netscape.com/NC-rdf#parent");
  if (!parentArc)        return(false);

  for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
  {
    var node = select_list[nodeIndex];
    if (!node)    break;
    var uri = node.getAttribute("ref");
    if ((uri) || (uri == ""))
    {
      uri = node.getAttribute("id");
    }
    if (!uri)    return(false);

    var rdfNode = rdf.GetResource(uri);
    if (!rdfNode)    break;

    // add node into selection array
    selectionArray.AppendElement(rdfNode);

    // get the parent's URI
    var parentURI="";
    var theParent = node;
    while (theParent)
    {
      theParent = theParent.parentNode;

      parentURI = theParent.getAttribute("ref");
      if ((!parentURI) || (parentURI == ""))
      {
        parentURI = theParent.getAttribute("id");
      }
      if (parentURI != "")  break;
    }
    if (parentURI == "")    return(false);

    var parentNode = rdf.GetResource(parentURI, true);
    if (!parentNode)  return(false);

    // add parent arc and node into arguments array
    argumentsArray.AppendElement(parentArc);
    argumentsArray.AppendElement(parentNode);
  }

  // do the command
  compositeDB.DoCommand( selectionArray, cmdResource, argumentsArray );

  dump("doContextCmd ends.\n\n");
  return(true);
}
