// -*- Mode: Java -*-

var sidebar_name    = '';   // Name for preferences (e.g. 'sidebar.<name>.foo')
var sidebar_uri     = '';   // Content to load in sidebar frame
var sidebar_width   = 0;    // Desired width of sidebar
var sidebar_pref    = '';   // Base for preferences (e.g. 'sidebar.browser')
var is_sidebar_open = false; 
var prefs           = null; // Handle to preference interface

function init_sidebar(name, uri, width) {
  sidebar_name  = name;
  sidebar_uri   = uri;
  sidebar_width = width;
  sidebar_pref  = 'sidebar.' + name;

  // Open/close sidebar based on saved pref.
  // This may be replaced by another system by hyatt.
  prefs = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch);
  if (prefs) {
    // The sidebar is closed by default, so open it only if the
    //    preference is set to true.
    try {
	    if (prefs.getBoolPref(sidebar_pref + '.open')) {
	      toggle_open_close();
	    }
    }
    catch (ex) {
	dump("failed to get sidebar_pref\n");
    }
  }
}

function toggle_open_close() {

  var sidebar = document.getElementById('sidebarframe');
  var grippy = document.getElementById('grippy');

  if (is_sidebar_open)
  {
    // Close it
    sidebar.setAttribute('style','visibility: hidden; width: 1px');
    sidebar.setAttribute('src','about:blank');

    grippy.setAttribute('open','');

    is_sidebar_open = false;
  }
  else
  {
    dump("Open it\n");
   
    sidebar.setAttribute('style', 'visibility: visible;width:' + sidebar_width + 'px');
    sidebar.setAttribute('src',   sidebar_uri);

    grippy.setAttribute('open','true');

    is_sidebar_open = true;
  }  

  try {
	  // Save new open/close state in prefs
	  if (prefs) {
	    prefs.setBoolPref(sidebar_pref + '.open', is_sidebar_open);
	  }
  }
  catch (ex) {
  	dump("failed to set the sidebar pref\n");
  }
}
