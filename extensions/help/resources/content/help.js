//-------- global variables

var gBrowser;
var key = {
 "?mail": "chrome://help/locale/mail_help.html",
 "?nav":  "chrome://help/locale/nav_help.html",
 "?im":    "chrome://help/locale/im_help.html",
 "?sec":   "chrome://help/locale/security_help.html",
 "?cust":  "chrome://help/locale/customize_help.html",
 "?comp":  "chrome://help/locale/composer_help.html",

 "?my_certs":  "chrome://help/locale/certs_help.html#My_Certificates",
 "?cert_backup_pwd":  "chrome://help/locale/certs_help.html#Choose_a_Certificate_Backup_Password",
 "?delete_my_certs":  "chrome://help/locale/certs_help.html#Delete_My_Certificate",
 "?change_pwd":  "chrome://help/locale/passwords_help.html#Change_Master_Password",
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
 "?no_cert":  "chrome://help/locale/cert_dialog_help.html#User_Identification_Request",
 "?new_ca":  "chrome://help/locale/cert_dialog_help.html#New_Certificate_Authority",
 "?new_web_cert":  "chrome://help/locale/cert_dialog_help.html#New_Web_Site_Certificate",
 "?exp_web_cert":  "chrome://help/locale/cert_dialog_help.html#Expired_Web_Site_Certificate",
 "?not_yet_web_cert":  "chrome://help/locale/cert_dialog_help.html#Web_Site_Certificate_Not_Yet_Valid",
 "?bad_name_web_cert":  "chrome://help/locale/cert_dialog_help.html#Unexpected_Certificate_Name",

 "?sec_gen":  "chrome://help/locale/privsec_help.html#privsec_help_first",
 "?ssl_prefs":  "chrome://help/locale/ssl_help.html#ssl_first",
 "?validation_prefs":  "chrome://help/locale/ssl_help.html#ssl_first",
 "?passwords_prefs":  "chrome://help/locale/ssl_help.html#ssl_first"
}


function init()
{
  // Initialize the Help window
  //  "window.arguments[0]" is undefined or context string

  // move to right end of screen
  var width = document.documentElement.getAttribute("width");
  var height = document.documentElement.getAttribute("height");
  window.moveTo(screen.availWidth-width, (screen.availHeight-height)/2);

  gBrowser = document.getElementById("help-content");
  var sessionHistory =  Components.classes["@mozilla.org/browser/shistory;1"]
                                  .createInstance(Components.interfaces.nsISHistory);

  getWebNavigation().sessionHistory = sessionHistory;

  //if ("argument" in window && window.arguments.length >= 1) {
  //  browser.loadURI(window.arguments[0]);
  //} else {
  //  goHome(); // should be able to do browser.goHome();
  //}

  if (window.location.search) {
      loadURI(key[window.location.search]);
      // selectTOC(key[window.location.search]);

  } else {
      goHome();
  }
}

function selectTOC(link_attr) {
  var items = document.getElementsByAttribute("helplink", link_attr);
  if (items.length >= 1) {
	var parentRow = items[0].parentNode;
  	var selectableNode = parentRow.parentNode; 	// helplink is an attribute
  							// on a treecell, which cannot be selected
  	var tree = document.getElementById("help-toc-tree");
  	tree.selectItem(selectableNode);
  } 
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

