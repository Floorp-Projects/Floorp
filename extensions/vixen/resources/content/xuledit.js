
const _DEBUG = true;
function _dd(aString)
{
  if (_DEBUG)
    dump("*** " + aString + "\n");
}

const kMenuHeight = 130;
const kPropertiesWidth = 170;


function xe_Startup()
{
  _dd("xe_Startup");
  window.moveTo(0,0);
  window.outerWidth = screen.availWidth;
  window.outerHeight = kMenuHeight;
  
  // initialise commands
  controllers.insertControllerAt(0, defaultController);
 
  // load a scratch document
  xe_LoadForm("chrome://xuledit/content/vfdScratch.xul");
  
  var url = Components.classes["component://netscape/network/standard-url"].createInstance();
  if (url) url = url.QueryInterface(Components.interfaces.nsIURI);
  url.spec = "chrome://xuledit/skin/vfdScratch.css";
  var chromeRegistry = Components.classes["component://netscape/chrome/chrome-registry"].getService();
  chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
  var url2 = chromeRegistry.convertChromeURL(url);
  dump(url2);
}

function xe_Shutdown()
{
  _dd("xe_Shutdown");
  
  const WM_PROGID = "component://netscape/rdf/datasource?name=window-mediator";
  var wm = nsJSComponentManager.getService(WM_PROGID, "nsIWindowMediator");
  var windows = wm.getXULWindowEnumerator("xuledit:document");
  while (windows.hasMoreElements()) {
    var currWindow = windows.getNext();
    currWindow.close();
  }
}

function xe_LoadForm(aURL)
{
  hwnd = openDialog(aURL, "", "chrome,dialog=no,resizable");
  hwnd.moveTo(kPropertiesWidth + 5, kMenuHeight + 5);
  hwnd.outerWidth = screen.availWidth - (kPropertiesWidth * 2) - 5;
  hwnd.outerHeight = screen.availHeight - kMenuHeight - 20;
}

var defaultController = {
  supportsCommand: function(command)
  {
    switch (command) {
    
    case "cmd_insert_button":
    case "cmd_insert_toolbarbutton":
    case "cmd_insert_menubutton":

    case "cmd_insert_toplevel_menu":
    case "cmd_insert_menu":
    case "cmd_insert_menuseparator":
    case "cmd_insert_menulist":
    case "cmd_insert_combobox":
    
    case "cmd_insert_textfield":
    case "cmd_insert_textarea":
    case "cmd_insert_rdf_editor":
    
    case "cmd_insert_static":
    case "cmd_insert_wrapping":
    case "cmd_insert_image":
    case "cmd_insert_browser":
    
    case "cmd_insert_box":
    case "cmd_insert_grid":
    case "cmd_insert_grid_row":
    case "cmd_insert_grid_col":
    case "cmd_insert_grid_spring":
    case "cmd_insert_splitter":
      return true;
      
    default:
      return false;
    }
  },
  
  isCommandEnabled: function(command)
  {
    switch (command) {    
    case "cmd_insert_button":
    case "cmd_insert_toolbarbutton":
    case "cmd_insert_menubutton":

    case "cmd_insert_toplevel_menu":
    case "cmd_insert_menu":
    case "cmd_insert_menuseparator":
    case "cmd_insert_menulist":
    case "cmd_insert_combobox":
    
    case "cmd_insert_textfield":
    case "cmd_insert_textarea":
    case "cmd_insert_rdf_editor":
    
    case "cmd_insert_static":
    case "cmd_insert_wrapping":
    case "cmd_insert_image":
    case "cmd_insert_browser":
    
    case "cmd_insert_box":
    case "cmd_insert_grid":
    case "cmd_insert_grid_row":
    case "cmd_insert_grid_col":
    case "cmd_insert_grid_spring":
    case "cmd_insert_splitter":
      return true;
    
    default:
      return false;
    }
  },
  
  doCommand: function(command)
  {
    switch (command)
    {
    
    case "cmd_insert_button":
      nsVFD.insertButtonElement("button");
      return true;
    case "cmd_insert_toolbarbutton":
    case "cmd_insert_menubutton":

    case "cmd_insert_toplevel_menu":
    case "cmd_insert_menu":
    case "cmd_insert_menuseparator":
    case "cmd_insert_menulist":
    case "cmd_insert_combobox":
    
    case "cmd_insert_textfield":
    case "cmd_insert_textarea":
    case "cmd_insert_rdf_editor":
    
    case "cmd_insert_static":
    case "cmd_insert_wrapping":
    case "cmd_insert_image":
    case "cmd_insert_browser":
    
    case "cmd_insert_box":
    case "cmd_insert_grid":
    case "cmd_insert_grid_row":
    case "cmd_insert_grid_col":
    case "cmd_insert_grid_spring":
    case "cmd_insert_splitter":
      return true;
      
    default:
      return false;
    }
  },
  
  onEvent: function(event)
  {
//    dump("DefaultController:onEvent\n");
  }
}
