//-------- global variables

var gBrowser;
var key = {
 "?mail": "chrome://help/locale/mail_help.html",
 "?nav":  "chrome://help/locale/nav_help.html",
 "?im":    "chrome://help/locale/im_help.html",
 "?sec":   "chrome://help/locale/security_help.html",
 "?cust":  "chrome://help/locale/customize_help.html",
 "?comp":  "chrome://help/locale/composer_help.html"
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

