


function fillContextMenu(name, treeName)
{
    if (!name)    return(false);
    var popupNode = document.getElementById(name);
    if (!popupNode)    return(false);
    // remove the menu node (which tosses all of its kids);
    // do this in case any old command nodes are hanging around
  while (popupNode.childNodes.length)
  {
    popupNode.removeChild(popupNode.childNodes[0]);
  }

    var treeNode = document.getElementById(treeName);
    if (!treeNode)    return(false);
    var db = treeNode.database;
    if (!db)    return(false);

    var compositeDB = db.QueryInterface(Components.interfaces.nsIRDFDataSource);
    if (!compositeDB)    return(false);

    var isupports = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
    if (!isupports)    return(false);
    var rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
    if (!rdf)    return(false);

    var target_item = document.popupNode.parentNode.parentNode;
    if (target_item && target_item.nodeName == "treeitem")
    {
      if (target_item.getAttribute('selected') != 'true') {
        treeNode.selectItem(target_item);
      }
    }

    var select_list = treeNode.selectedItems;

    var separatorResource = rdf.GetResource("http://home.netscape.com/NC-rdf#BookmarkSeparator");
    if (!separatorResource) return(false);

    // perform intersection of commands over selected nodes
    var cmdArray = new Array();
    var selectLength = select_list.length;
    if (selectLength == 0)  selectLength = 1;
    for (var nodeIndex=0; nodeIndex < selectLength; nodeIndex++)
    {
      var id = null;

  // if nothing is selected, get commands for the "ref" of the tree root
      if (select_list.length == 0)
      {
    id = treeNode.getAttribute("ref");
          if (!id)    break;
      }
      else
      {
          var node = select_list[nodeIndex];
          if (!node)    break;
          id = node.getAttribute("id");
          if (!id)    break;
  }

        var rdfNode = rdf.GetResource(id);
        if (!rdfNode)    break;
        var cmdEnum = db.GetAllCmds(rdfNode);
        if (!cmdEnum)    break;

        var nextCmdArray = new Array();
        while (cmdEnum.hasMoreElements())
        {
            var cmd = cmdEnum.getNext();
            if (!cmd)    break;
            if (nodeIndex == 0)
            {
                cmdArray[cmdArray.length] = cmd;
            }
            else
            {
                nextCmdArray[cmdArray.length] = cmd;
            }
        }
        if (nodeIndex > 0)
        {
            // perform command intersection calculation
            for (var cmdIndex = 0; cmdIndex < cmdArray.length; cmdIndex++)
            {
                var    cmdFound = false;
                for (var nextCmdIndex = 0; nextCmdIndex < nextCmdArray.length; nextCmdIndex++)
                {
                    if (nextCmdArray[nextCmdIndex] == cmdArray[cmdIndex])
                    {
                        cmdFound = true;
                        break;
                    }
                }
                if ((cmdFound == false) && (cmdArray[cmdIndex]))
                {
      var cmdResource = cmdArray[cmdIndex].QueryInterface(Components.interfaces.nsIRDFResource);
                  if ((cmdResource) && (cmdResource != separatorResource))
                  {
        cmdArray[cmdIndex] = null;
      }
                }
            }
        }
    }

    // need a resource to ask RDF for each command's name
    var rdfNameResource = rdf.GetResource("http://home.netscape.com/NC-rdf#Name");
    if (!rdfNameResource)        return(false);

    // build up menu items
    if (cmdArray.length < 1)    return(false);

    var lastWasSep = false;

    for (var cmdIndex = 0; cmdIndex < cmdArray.length; cmdIndex++)
    {
        var cmd = cmdArray[cmdIndex];
        if (!cmd)        continue;
        var cmdResource = cmd.QueryInterface(Components.interfaces.nsIRDFResource);
        if (!cmdResource)    break;

  // handle separators
  if (cmdResource == separatorResource)
  {
    if (lastWasSep != true)
    {
      lastWasSep = true;
            var menuItem = document.createElement("menuseparator");
            popupNode.appendChild(menuItem);
    }
    continue;
  }

  lastWasSep = false;

        var cmdNameNode = compositeDB.GetTarget(cmdResource, rdfNameResource, true);
        if (!cmdNameNode)    break;
        cmdNameLiteral = cmdNameNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (!cmdNameLiteral)    break;
        cmdName = cmdNameLiteral.Value;
        if (!cmdName)        break;

        var menuItem = document.createElement("menuitem");
        menuItem.setAttribute("label", cmdName);
        popupNode.appendChild(menuItem);
        // work around bug # 26402 by setting "oncommand" attribute AFTER appending menuitem
        menuItem.setAttribute("oncommand", "return doContextCmd('" + cmdResource.Value + "', '" + treeName + "');");
    }

  // strip off leading/trailing menuseparators
  while (popupNode.childNodes.length > 0)
  {
    if (popupNode.childNodes[0].tagName != "menuseparator")
      break;
    popupNode.removeChild(popupNode.childNodes[0]);
  }
  while (popupNode.childNodes.length > 0)
  {
    if (popupNode.childNodes[popupNode.childNodes.length - 1].tagName != "menuseparator")
      break;
    popupNode.removeChild(popupNode.childNodes[popupNode.childNodes.length - 1]);
  }

  var   searchMode = 0;
  if (pref) searchMode = pref.getIntPref("browser.search.mode");
  if (pref && bundle)
  {
    // then add a menu separator (if necessary)
    if (popupNode.childNodes.length > 0)
    {
      if (popupNode.childNodes[popupNode.childNodes.length - 1].tagName != "menuseparator")
      {
          var menuSep = document.createElement("menuseparator");
          popupNode.appendChild(menuSep);
      }
    }
    // And then add a "Search Mode" menu item
    var propMenuName = (searchMode == 0) ? bundle.GetStringFromName("enableAdvanced") : bundle.GetStringFromName("disableAdvanced");
    var menuItem = document.createElement("menuitem");
    menuItem.setAttribute("label", propMenuName);
    popupNode.appendChild(menuItem);
    // Work around bug # 26402 by setting "oncommand" attribute
    // AFTER appending menuitem
    menuItem.setAttribute("oncommand", "return setSearchMode();");
  }

    return(true);
}



function setSearchMode()
{
  var   searchMode = 0;
  if (pref) searchMode = pref.getIntPref("browser.search.mode");
  if (searchMode == 0)  searchMode = 1;
  else      searchMode = 0;
  if (pref) pref.setIntPref("browser.search.mode", searchMode);
  return(true);
}



function doContextCmd(cmdName, treeName)
{
  var treeNode = document.getElementById(treeName);
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

  // set up selection nsISupportsArray
  var selectionInstance = Components.classes["@mozilla.org/supports-array;1"].createInstance();
  var selectionArray = selectionInstance.QueryInterface(Components.interfaces.nsISupportsArray);

  // set up arguments nsISupportsArray
  var argumentsInstance = Components.classes["@mozilla.org/supports-array;1"].createInstance();
  var argumentsArray = argumentsInstance.QueryInterface(Components.interfaces.nsISupportsArray);

  // get argument (parent)
  var parentArc = rdf.GetResource("http://home.netscape.com/NC-rdf#parent");
  if (!parentArc)        return(false);

  var select_list = treeNode.selectedItems;
  if (select_list.length < 1)
  {
    // if nothing is selected, default to using the "ref" on the root of the tree
    var uri = treeNode.getAttribute("ref");
    if (!uri || uri=="")    return(false);
    var rdfNode = rdf.GetResource(uri);
    // add node into selection array
    if (rdfNode)
    {
      selectionArray.AppendElement(rdfNode);
    }
  }
  else for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
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
  return(true);
}



/* Note: doSort() does NOT support natural order sorting, unless naturalOrderResource is valid,
         in which case we sort ascending on naturalOrderResource
 */
function doSort(sortColName, naturalOrderResource)
{
  var node = document.getElementById(sortColName);
  // determine column resource to sort on
  var sortResource = node.getAttribute('resource');
  if (!sortResource) return(false);

  var sortDirection="ascending";
  var isSortActive = node.getAttribute('sortActive');
  if (isSortActive == "true")
  {
    sortDirection = "ascending";

    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
    {
      if (sortResource != naturalOrderResource)
      {
        sortDirection = "descending";
      }
    }
    else if (currentDirection == "descending")
    {
      if (naturalOrderResource != null && naturalOrderResource != "")
      {
        sortResource = naturalOrderResource;
      }
    }
  }

  var isupports = Components.classes["@mozilla.org/xul/xul-sort-service;1"].getService();
  if (!isupports)    return(false);
  var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
  if (!xulSortService)    return(false);
  try
  {
    xulSortService.Sort(node, sortResource, sortDirection);
  }
  catch(ex)
  {
    debug("Exception calling xulSortService.Sort()");
  }
  return(true);
}



function setInitialSort(node, sortDirection)
{
  // determine column resource to sort on
  var sortResource = node.getAttribute('resource');
  if (!sortResource) return(false);

  try
  {
    var isupports = Components.classes["@mozilla.org/xul/xul-sort-service;1"].getService();
    if (!isupports)    return(false);
    var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
    if (!xulSortService)    return(false);
    xulSortService.Sort(node, sortResource, sortDirection);
  }
  catch(ex)
  {
  }
  return(true);
}
