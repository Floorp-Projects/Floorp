//-------- global variables

var gBrowser;

// The key object is used to define special context strings that, when appended to
// the url for the help window itself, load specific content. For example, the uri:
//     chrome://help/content/help.xul?mail
// loads the document at:
//     chrome://help/locale/mail_help.html into the window on initialization.
var key = {
 "?mail": "chrome://help/locale/mail_help.html",
 "?nav":  "chrome://help/locale/nav_help.html",
 "?im":    "chrome://help/locale/im_help.html",
 "?sec":   "chrome://help/locale/security_help.html",
 "?cust":  "chrome://help/locale/customize_help.html",
 "?comp":  "chrome://help/locale/composer_help.html",
 "?prefs":   "chrome://help/locale/prefs_help.html#prefs_first",

 "?mail_prefs_general": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_MAIN_PANE", 
 "?mail_prefs_display": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_DISPLAY", 
 "?mail_prefs_messages": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_MESSAGES", 
 "?mail_prefs_formatting": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_FORMATTING", 
 "?mail_prefs_addressing": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_ADDRESSING", 
 "?mail_prefs_offline": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_OFFLINE", 

 "?composer_prefs_general": "chrome://help/locale/composer_help.html#PREFERENCES_EDITOR_GENERAL", 
 "?composer_prefs_newpage": "chrome://help/locale/composer_help.html#PREFERENCES_NEWPAGE", 

 "?appearance_pref":  "chrome://help/locale/cs_nav_prefs_appearance.html",
 "?appearance_pref_appearance":  "chrome://help/locale/cs_nav_prefs_appearance.html#appearance",
 "?appearance_pref_fonts":  "chrome://help/locale/cs_nav_prefs_appearance.html#fonts",
 "?appearance_pref_colors":  "chrome://help/locale/cs_nav_prefs_appearance.html#colors",
 "?appearance_pref_fonts":  "chrome://help/locale/cs_nav_prefs_appearance.html#fonts",
 "?appearance_pref_themes":  "chrome://help/locale/cs_nav_prefs_appearance.html#themes",

 "?navigator_pref":  "chrome://help/locale/cs_nav_prefs_navigator.html",
 "?navigator_pref_navigator":  "chrome://help/locale/cs_nav_prefs_navigator.html#Navigator",
 "?navigator_pref_history":  "chrome://help/locale/cs_nav_prefs_navigator.html#History",
 "?navigator_pref_languages":  "chrome://help/locale/cs_nav_prefs_navigator.html#Languages",
 "?navigator_pref_helper_applications": "chrome://help/locale/cs_nav_prefs_navigator.html#Helper",
 "?navigator_pref_internet_searching": "chrome://help/locale/cs_nav_prefs_navigator.html#Internet",
 "?navigator_pref_smart_browsing": "chrome://help/locale/cs_nav_prefs_navigator.html#Smart",  

 "?advanced_pref":  "chrome://help/locale/cs_nav_prefs_advanced.html",
 "?advanced_pref_advanced":  "chrome://help/locale/cs_nav_prefs_advanced.html#Advanced",
 "?advanced_pref_cache":  "chrome://help/locale/cs_nav_prefs_advanced.html#Cache",
 "?advanced_pref_installation": "chrome://help/locale/cs_nav_prefs_advanced.html#Software_Installation",
 "?advanced_pref_mouse_wheel": "chrome://help/locale/cs_nav_prefs_advanced.html#Mouse_Wheel",
 "?advanced_pref_system":  "chrome://help/locale/cs_nav_prefs_advanced.html#system",
"?advanced_pref_proxies": "chrome://help/locale/cs_nav_prefs_advanced.html#Proxies",
 "?nover_noencrypt":  "chrome://help/locale/ssl_page_info_help.html#Not_Verified_Not Encrypted",
 "?ver_encrypt":  "chrome://help/locale/ssl_page_info_help.html#Verified_Encrypted",
 "?conver_encrypt":  "chrome://help/locale/ssl_page_info_help.html#Conditionally_Verified_Encrypted",
 "?ver_noencrypt":  "chrome://help/locale/ssl_page_info_help.html#Verified_Not Encrypted",
 "?conver_noencrypt":  "chrome://help/locale/ssl_page_info_help.html#Conditionally_Verified_Not_Encrypted",

 "?my_certs":  "chrome://help/locale/certs_help.html#My_Certificates",
 "?cert_backup_pwd":  "chrome://help/locale/certs_help.html#Choose_a_Certificate_Backup_Password",
 "?delete_my_certs":  "chrome://help/locale/certs_help.html#Delete_My_Certificate",
 "?change_pwd":  "chrome://help/locale/passwords_help.html#Change_Master_Password",
 "?reset_pwd":  "chrome://help/locale/passwords_help.html#Reset_Master_Password",
 "?web_certs":  "chrome://help/locale/certs_help.html#Web_Site_Certificates",
 "?edit_web_certs":  "chrome://help/locale/certs_help.html#Edit_Web_Site_Certificate_Settings",
 "?delete_web_certs":  "chrome://help/locale/certs_help.html#Delete_Web_Site_Certificate",
 "?ca_certs":  "chrome://help/locale/certs_help.html#CA_Certificates",
 "?edit_ca_certs":  "chrome://help/locale/certs_help.html#Edit_CA_Certificate_Settings",
 "?delete_ca_certs":  "chrome://help/locale/certs_help.html#Delete_CA_Certificate",
 "?sec_devices":  "chrome://help/locale/certs_help.html#Security_Devices",

 "?cert_details":  "chrome://help/locale/cert_dialog_help.html#Certificate_Details",
 "?which_token":  "chrome://help/locale/cert_dialog_help.html#Choose_Security_Device",
 "?priv_key_copy":  "chrome://help/locale/cert_dialog_help.html#Encryption_Key_Copy",
 "?backup_your_cert":  "chrome://help/locale/cert_dialog_help.html#Certificate_Backup",
 "?which_cert":  "chrome://help/locale/cert_dialog_help.html#User_Identification_Request",
 "?no_cert":  "chrome://help/locale/cert_dialog_help.html#No_Acceptable_Identification",
 "?new_ca":  "chrome://help/locale/cert_dialog_help.html#New_Certificate_Authority",
 "?exp_crl":  "chrome://help/locale/cert_dialog_help.html#old_next_update_crl",
 "?new_web_cert":  "chrome://help/locale/cert_dialog_help.html#New_Web_Site_Certificate",
 "?exp_web_cert":  "chrome://help/locale/cert_dialog_help.html#Expired_Web_Site_Certificate",
 "?not_yet_web_cert":  "chrome://help/locale/cert_dialog_help.html#Web_Site_Certificate_Not_Yet_Valid",
 "?bad_name_web_cert":  "chrome://help/locale/cert_dialog_help.html#Unexpected_Certificate_Name",

 "?sec_gen":  "chrome://help/locale/privsec_help.html#privsec_help_first",
 "?ssl_prefs":  "chrome://help/locale/ssl_help.html#ssl_first",
 "?validation_prefs":  "chrome://help/locale/validation_help.html",
 "?passwords_prefs":  "chrome://help/locale/passwords_help.html",
 "?passwords_master":  "chrome://help/locale/passwords_help.html#Master_Password_Timeout",
 "?forms_prefs":  "chrome://help/locale/using_priv_help.html#forms_prefs",  
 "?cookies_prefs": "chrome://help/locale/using_priv_help.html#cookie_prefs",
 "?images_prefs": "chrome://help/locale/using_priv_help.html#using_images",
 "?certs_prefs": "chrome://help/locale/certs_prefs_help.html",

 "?im_prefs_general": "chrome://help/locale/im_help.html#im_gen", 
 "?im_prefs_privacy": "chrome://help/locale/im_help.html#im_privacy", 
 "?im_prefs_notification": "chrome://help/locale/im_help.html#im_notif", 
 "?im_prefs_away": "chrome://help/locale/im_help.html#im_awaypref", 
 "?im_prefs_connection": "chrome://help/locale/im_help.html#im_connect", 
 
 "?image_properties": "chrome://help/locale/composer_help.html#PROPERTIES_IMAGE",
 "?imagemap_properties": "chrome://help/locale/composer_help.html#IMAGE_MAPS",
 "?link_properties": "chrome://help/locale/composer_help.html#page_link",
 "?table_properties": "chrome://help/locale/composer_help.html#Setting_table_properties",
 "?advanced_property_editor": "chrome://help/locale/composer_help.html#PROPERTY_EDITOR",

 // Mailnews account settings tag mappings. 
 "?mail_account_identity": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_IDENTITY",
 "?mail_server_imap": "chrome://help/locale/mail_help.html#IMAP_SERVER", 
 "?mail_server_pop3": "chrome://help/locale/mail_help.html#POP_SERVER", 
 "?mail_server_nntp": "chrome://help/locale/mail_help.html#DISCUSSION_HOST_PROPERTIES", 
 "?mail_copies": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_COPIES",
 "?mail_addressing_settings": "chrome://help/locale/mail_help.html#addressing_settings", 
 "?mail_offline_imap": "chrome://help/locale/mail_help.html#OFFLINE_IMAP", 
 "?mail_offline_pop3": "chrome://help/locale/mail_help.html#OFFLINE_POP", 
 "?mail_offline_nntp": "chrome://help/locale/mail_help.html#OFFLINE_NEWS", 
 "?mail_smtp": "chrome://help/locale/mail_help.html#PREFERENCES_MAILNEWS_SMTP" 
}

// This function is called by dialogs that want to display context-sensitive help
function openHelp(uri)
{
  var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
  var topWindow = windowManagerInterface.getMostRecentWindow( "mozilla:help" );
  if ( topWindow ) {
     topWindow.focus();
  } else {
      window.open(uri, "_blank", "chrome,menubar,toolbar,dialog=no,resizable,scrollbars");
  }
}

function init()
{
  // Initialize the Help window

  // move to right end of screen
  var width = document.documentElement.getAttribute("width");
  var height = document.documentElement.getAttribute("height");
  window.moveTo(screen.availWidth-width, (screen.availHeight-height)/2);

  gBrowser = document.getElementById("help-content");
  var sessionHistory =  Components.classes["@mozilla.org/browser/shistory;1"]
                                  .createInstance(Components.interfaces.nsISHistory);

  getWebNavigation().sessionHistory = sessionHistory;
  // if this is context-sensitive,
  // then window.location.search contains the "key" (from the top of this file)
  // that resolves to a particular html document or target
  if (key[window.location.search]) {
      dump("loading help content: " + key[window.location.search] + "\n");
      loadURI(key[window.location.search]);
   } else {
      goHome();
  }
  window.XULBrowserWindow = new nsHelpStatusHandler();
  // hook up UI through progress listener
  var browser = document.getElementById("help-content");
  var interfaceRequestor = browser.docShell.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
  var webProgress = interfaceRequestor.getInterface(Components.interfaces.nsIWebProgress);
  webProgress.addProgressListener(window.XULBrowserWindow);
}

// select the item in the tree called "Dialog Help" if the window was loaded from a dialog
function setContext() {
  var items = document.getElementsByAttribute("helplink", "chrome://help/locale/context_help.html");
  if (items.length) {
     var tree = document.getElementById("help-toc-tree");
     try { tree.selectItem(items[0].parentNode.parentNode); } catch(ex) { dump("can't select in toc: " + ex + "\n"); }
  }
}

function selectTOC(link_attr) {
  var items = document.getElementsByAttribute("helplink", link_attr);
  if (items.length) {
     openTwistyTo(items[0]);
     var tree = document.getElementById("help-toc-tree");
     try { tree.selectItem(items[0].parentNode.parentNode); } catch(ex) { dump("can't select in toc: " + ex + "\n"); }
  } 
}

// open parent nodes for the selected node
// until you get to the top of the tree
function openTwistyTo(selectedURINode)
{
  var parent = selectedURINode.parentNode;
  var tree = document.getElementById("help-toc-tree");
  if (parent == tree)
    return;
 
  parent.setAttribute("open", "true");
  openTwistyTo(parent);
}


function getWebNavigation()
{
  return gBrowser.webNavigation;
}

function loadURI(aURI)
{
  const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
  getWebNavigation().loadURI(aURI, nsIWebNavigation.LOAD_FLAGS_NONE);
}

function goBack()
{
  var webNavigation = getWebNavigation();
  if (webNavigation.canGoBack)
    webNavigation.goBack();
}

function goForward()
{
  var webNavigation = getWebNavigation();
  if (webNavigation.canGoForward)
    webNavigation.goForward();
}

function goHome() {
  // load "Welcome" page
  loadURI("chrome://help/locale/welcome_help.html");
}

function print()
{
  try {
    _content.print();
  } catch (e) {
  }
}

function createBackMenu(event)
{
  return FillHistoryMenu(event.target, "back");
}

function createForwardMenu(event)
{
  return FillHistoryMenu(event.target, "forward");
}

function gotoHistoryIndex(aEvent)
{
  var index = aEvent.target.getAttribute("index");
  if (!index)
    return false;
  try {
    getWebNavigation().gotoIndex(index);
  }
  catch(ex) {
    return false;
  }
  return true;
}

function BrowserBack()
{
  try {
    getWebNavigation().goBack();
  }
  catch(ex) {
  }
  UpdateBackForwardButtons();
}

function BrowserForward()
{
  try {
    getWebNavigation().goForward();
  }
  catch(ex) {
  }
  UpdateBackForwardButtons();
}

function nsHelpStatusHandler()
{
}

nsHelpStatusHandler.prototype =
{
  onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus) {},
  onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress,
                              aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},
  onSecurityChange : function(aWebProgress, aRequest, state) {},
  onLocationChange : function(aWebProgress, aRequest, aLocation)
  {
    UpdateBackForwardButtons();
  },
  QueryInterface : function(aIID)
  {
    if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
      aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
      aIID.equals(Components.interfaces.nsIXULBrowserWindow) ||
      aIID.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },
  setJSStatus : function(status) {},
  setJSDefaultStatus : function(status) {},
  setOverLink : function(link) {}
}

function UpdateBackForwardButtons()
{
  var backBroadcaster = document.getElementById("canGoBack");
  var forwardBroadcaster = document.getElementById("canGoForward");
  var webNavigation = getWebNavigation();

  // Avoid setting attributes on broadcasters if the value hasn't changed!
  // Remember, guys, setting attributes on elements is expensive!  They
  // get inherited into anonymous content, broadcast to other widgets, etc.!
  // Don't do it if the value hasn't changed! - dwh

  var backDisabled = (backBroadcaster.getAttribute("disabled") == "true");
  var forwardDisabled = (forwardBroadcaster.getAttribute("disabled") == "true");

  if (backDisabled == webNavigation.canGoBack)
    backBroadcaster.setAttribute("disabled", !backDisabled);
  
  if (forwardDisabled == webNavigation.canGoForward)
    forwardBroadcaster.setAttribute("disabled", !forwardDisabled);
}

function find(again)
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  var browser = document.getElementById("help-content");
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;
  if (again)
    findAgainInPage(browser, window._content, focusedWindow);
  else
    findInPage(browser, window._content, focusedWindow)
}

function getMarkupDocumentViewer()
{
  return document.getElementById("help-content").markupDocumentViewer;
}

function BrowserReload()
{
  const reloadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  return BrowserReloadWithFlags(reloadFlags);
}

function BrowserReloadWithFlags(reloadFlags)
{
   try {
     /* Need to get SessionHistory from docshell so that
      * reload on framed pages will work right. This 
      * method should not be used for the context menu item "Reload frame".
      * "Reload frame" should directly call into docshell as it does right now
      */
     var sh = getWebNavigation().sessionHistory;
     var webNav = sh.QueryInterface(Components.interfaces.nsIWebNavigation);      
     webNav.reload(reloadFlags);
   }
   catch(ex) {
   }
 }
 
 function BrowserPageInfo(doc)
 {
   window.openDialog("chrome://navigator/content/pageInfo.xul",
                     "_blank",
                     "chrome,dialog=no",
                     doc);
}

function BrowserViewSource()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (focusedWindow == window)
    focusedWindow = _content;

  if (focusedWindow)
    var docCharset = "charset=" + focusedWindow.document.characterSet;

  BrowserViewSourceOfURL(_content.location, docCharset);
}

function BrowserViewSourceOfURL(url, charset)
{
  // try to open a view-source window while inheriting the charset (if any)
  openDialog("chrome://navigator/content/viewSource.xul",
             "_blank",
             "scrollbars,resizable,chrome,dialog=no",
             url, charset);
}

function getBrowser() {
  return document.getElementById("help-content");
}