
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  
var nsVFD = {

  insertButtonElement: function (aButtonElementType) 
  {
    switch (aButtonElementType) {
    case "button":
      var button = document.createElementNS(XUL_NS, "button");
      // insert some smarts to autogenerate button labels
      button.setAttribute("value", "Button1");
      button.setAttribute("flex", "1");
      button.setAttribute("crop", "right");

      this.genericInsertElement(button);
      break;
    case "toolbar-button":
      break;
    case "menu-button":
      break;
    }
  },

  genericInsertElement: function (aElement) 
  {
    var domDocument = getDocument();
    
    // get the focused element so we know where to insert
    
    // otherwise, just use the window
    var scratchWindow = getDocumentWindow(domDocument);
    
    scratchWindow.appendChild(aElement);
  },
};

function getDocumentWindow(aDocument)
{ 
  if (!aDocument) 
    aDocument = getDocument();
  
  for (var i = 0; i < aDocument.childNodes.length; i++)
    if (aDocument.childNodes[i].localName == "window")
      return aDocument.childNodes[i];
  return null;
}

function getDocument()
{ 
  const WM_PROGID = "component://netscape/rdf/datasource?name=window-mediator";
  var wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
  return wm.getMostRecentWindow("xuledit:document").frames["vfView"].document;
}