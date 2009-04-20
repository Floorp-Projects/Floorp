////////////////////////////////////////////////////////////////////////////////
// Interfaces

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;

const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessibleStateChangeEvent =
  Components.interfaces.nsIAccessibleStateChangeEvent;
const nsIAccessibleCaretMoveEvent =
  Components.interfaces.nsIAccessibleCaretMoveEvent;

const nsIAccessibleStates = Components.interfaces.nsIAccessibleStates;
const nsIAccessibleRole = Components.interfaces.nsIAccessibleRole;
const nsIAccessibleTypes = Components.interfaces.nsIAccessibleTypes;

const nsIAccessibleRelation = Components.interfaces.nsIAccessibleRelation;

const nsIAccessNode = Components.interfaces.nsIAccessNode;
const nsIAccessible = Components.interfaces.nsIAccessible;

const nsIAccessibleCoordinateType =
      Components.interfaces.nsIAccessibleCoordinateType;

const nsIAccessibleDocument = Components.interfaces.nsIAccessibleDocument;

const nsIAccessibleText = Components.interfaces.nsIAccessibleText;
const nsIAccessibleEditableText = Components.interfaces.nsIAccessibleEditableText;

const nsIAccessibleHyperLink = Components.interfaces.nsIAccessibleHyperLink;
const nsIAccessibleHyperText = Components.interfaces.nsIAccessibleHyperText;

const nsIAccessibleImage = Components.interfaces.nsIAccessibleImage;
const nsIAccessibleSelectable = Components.interfaces.nsIAccessibleSelectable;
const nsIAccessibleTable = Components.interfaces.nsIAccessibleTable;
const nsIAccessibleValue = Components.interfaces.nsIAccessibleValue;

const nsIObserverService = Components.interfaces.nsIObserverService;

const nsIDOMDocument = Components.interfaces.nsIDOMDocument;
const nsIDOMEvent = Components.interfaces.nsIDOMEvent;
const nsIDOMHTMLDocument = Components.interfaces.nsIDOMHTMLDocument;
const nsIDOMNode = Components.interfaces.nsIDOMNode;
const nsIDOMWindow = Components.interfaces.nsIDOMWindow;

const nsIPropertyElement = Components.interfaces.nsIPropertyElement;

////////////////////////////////////////////////////////////////////////////////
// States

const STATE_CHECKED = nsIAccessibleStates.STATE_CHECKED;
const STATE_CHECKABLE = nsIAccessibleStates.STATE_CHECKABLE;
const STATE_COLLAPSED = nsIAccessibleStates.STATE_COLLAPSED;
const STATE_EXPANDED = nsIAccessibleStates.STATE_EXPANDED;
const STATE_EXTSELECTABLE = nsIAccessibleStates.STATE_EXTSELECTABLE;
const STATE_FOCUSABLE = nsIAccessibleStates.STATE_FOCUSABLE;
const STATE_FOCUSED = nsIAccessibleStates.STATE_FOCUSED;
const STATE_HASPOPUP = nsIAccessibleStates.STATE_HASPOPUP;
const STATE_LINKED = nsIAccessibleStates.STATE_LINKED;
const STATE_MIXED = nsIAccessibleStates.STATE_MIXED;
const STATE_MULTISELECTABLE = nsIAccessibleStates.STATE_MULTISELECTABLE;
const STATE_PRESSED = nsIAccessibleStates.STATE_PRESSED;
const STATE_READONLY = nsIAccessibleStates.STATE_READONLY;
const STATE_SELECTABLE = nsIAccessibleStates.STATE_SELECTABLE;
const STATE_SELECTED = nsIAccessibleStates.STATE_SELECTED;
const STATE_TRAVERSED = nsIAccessibleStates.STATE_TRAVERSED;
const STATE_UNAVAILABLE = nsIAccessibleStates.STATE_UNAVAILABLE;

const EXT_STATE_EDITABLE = nsIAccessibleStates.EXT_STATE_EDITABLE;
const EXT_STATE_EXPANDABLE = nsIAccessibleStates.EXT_STATE_EXPANDABLE;
const EXT_STATE_HORIZONTAL = nsIAccessibleStates.EXT_STATE_HORIZONTAL;
const EXT_STATE_INVALID = nsIAccessibleStates.STATE_INVALID;
const EXT_STATE_MULTI_LINE = nsIAccessibleStates.EXT_STATE_MULTI_LINE;
const EXT_STATE_REQUIRED = nsIAccessibleStates.STATE_REQUIRED;
const EXT_STATE_SUPPORTS_AUTOCOMPLETION = 
      nsIAccessibleStates.EXT_STATE_SUPPORTS_AUTOCOMPLETION;

////////////////////////////////////////////////////////////////////////////////
// OS detect
const MAC = (navigator.platform.indexOf("Mac") != -1)? true : false;
const LINUX = (navigator.platform.indexOf("Linux") != -1)? true : false;
const WIN = (navigator.platform.indexOf("Win") != -1)? true : false;

////////////////////////////////////////////////////////////////////////////////
// Accessible general

/**
 * nsIAccessibleRetrieval, initialized when test is loaded.
 */
var gAccRetrieval = null;

/**
 * Invokes the given function when document is loaded. Preferable to mochitests
 * 'addLoadEvent' function -- additionally ensures state of the document
 * accessible is not busy.
 *
 * @param aFunc  the function to invoke
 */
function addA11yLoadEvent(aFunc)
{
  function waitForDocLoad()
  {
    window.setTimeout(
      function()
      {
        var accDoc = getAccessible(document);
        var state = {};
        accDoc.getState(state, {});
        if (state.value & nsIAccessibleStates.STATE_BUSY)
          return waitForDocLoad();

        aFunc.call();
      },
      200
    );
  }

  addLoadEvent(waitForDocLoad);
}

////////////////////////////////////////////////////////////////////////////////
// Get DOM node/accesible helpers

/**
 * Return the DOM node.
 */
function getNode(aNodeOrID)
{
  if (!aNodeOrID)
    return null;

  var node = aNodeOrID;

  if (!(aNodeOrID instanceof nsIDOMNode)) {
    node = document.getElementById(aNodeOrID);

    if (!node) {
      ok(false, "Can't get DOM element for " + aNodeOrID);
      return null;
    }
  }

  return node;
}

/**
 * Constants indicates getAccessible doesn't fail if there is no accessible.
 */
const DONOTFAIL_IF_NO_ACC = 1;

/**
 * Constants indicates getAccessible won't fail if accessible doesn't implement
 * the requested interfaces.
 */
const DONOTFAIL_IF_NO_INTERFACE = 2;

/**
 * Return accessible for the given identifier (may be ID attribute or DOM
 * element or accessible object).
 *
 * @param aAccOrElmOrID      [in] identifier to get an accessible implementing
 *                           the given interfaces
 * @param aInterfaces        [in, optional] the interface or an array interfaces
 *                           to query it/them from obtained accessible
 * @param aElmObj            [out, optional] object to store DOM element which
 *                           accessible is obtained for
 * @param aDoNotFailIf       [in, optional] no error for special cases (see
 *                            constants above)
 */
function getAccessible(aAccOrElmOrID, aInterfaces, aElmObj, aDoNotFailIf)
{
  if (!aAccOrElmOrID)
    return;

  var elm = null;

  if (aAccOrElmOrID instanceof nsIAccessible) {
    aAccOrElmOrID.QueryInterface(nsIAccessNode);
    elm = aAccOrElmOrID.DOMNode;

  } else if (aAccOrElmOrID instanceof nsIDOMNode) {
    elm = aAccOrElmOrID;

  } else {
    var elm = document.getElementById(aAccOrElmOrID);
    if (!elm) {
      ok(false, "Can't get DOM element for " + aAccOrElmOrID);
      return null;
    }
  }

  if (aElmObj && (typeof aElmObj == "object"))
    aElmObj.value = elm;

  var acc = (aAccOrElmOrID instanceof nsIAccessible) ? aAccOrElmOrID : null;
  if (!acc) {
    try {
      acc = gAccRetrieval.getAccessibleFor(elm);
    } catch (e) {
    }

    if (!acc) {
      if (!(aDoNotFailIf & DONOTFAIL_IF_NO_ACC))
        ok(false, "Can't get accessible for " + aAccOrElmOrID);

      return null;
    }
  }

  if (!aInterfaces)
    return acc;

  if (aInterfaces instanceof Array) {
    for (var index = 0; index < aInterfaces.length; index++) {
      try {
        acc.QueryInterface(aInterfaces[index]);
      } catch (e) {
        if (!(aDoNotFailIf & DONOTFAIL_IF_NO_INTERFACE))
          ok(false, "Can't query " + aInterfaces[index] + " for " + aID);

        return null;
      }
    }
    return acc;
  }
  
  try {
    acc.QueryInterface(aInterfaces);
  } catch (e) {
    ok(false, "Can't query " + aInterfaces + " for " + aID);
    return null;
  }
  
  return acc;
}

/**
 * Return true if the given identifier has an accessible, or exposes the wanted
 * interfaces.
 */
function isAccessible(aAccOrElmOrID, aInterfaces)
{
  return getAccessible(aAccOrElmOrID, aInterfaces, null,
                       DONOTFAIL_IF_NO_ACC | DONOTFAIL_IF_NO_INTERFACE) ?
    true : false;
}

/**
 * Run through accessible tree of the given identifier so that we ensure
 * accessible tree is created.
 */
function ensureAccessibleTree(aAccOrElmOrID)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return;

  var child = acc.firstChild;
  while (child) {
    ensureAccessibleTree(child);
    try {
      child = child.nextSibling;
    } catch (e) {
      child = null;
    }
  }
}

/**
 * Compare expected and actual accessibles trees.
 */
function testAccessibleTree(aAccOrElmOrID, aAccTree)
{
  var acc = getAccessible(aAccOrElmOrID);
  if (!acc)
    return;

  for (var prop in aAccTree) {
    var msg = "Wrong value of property '" + prop + "'.";
    if (prop == "role")
      is(roleToString(acc[prop]), roleToString(aAccTree[prop]), msg);
    else if (prop != "children")
      is(acc[prop], aAccTree[prop], msg);
  }

  if ("children" in aAccTree) {
    var children = acc.children;
    is(aAccTree.children.length, children.length,
       "Different amount of expected children.");

    if (aAccTree.children.length == children.length) { 
      for (var i = 0; i < children.length; i++) {
        var child = children.queryElementAt(i, nsIAccessible);
        testAccessibleTree(child, aAccTree.children[i]);
      }
    }
  }
}


/**
 * Convert role to human readable string.
 */
function roleToString(aRole)
{
  return gAccRetrieval.getStringRole(aRole);
}

/**
 * Convert states to human readable string.
 */
function statesToString(aStates, aExtraStates)
{
  var list = gAccRetrieval.getStringStates(aStates, aExtraStates);

  var str = "";
  for (var index = 0; index < list.length; index++)
    str += list.item(index) + ", ";

  return str;
}

/**
 * Convert event type to human readable string.
 */
function eventTypeToString(aEventType)
{
  gAccRetrieval.getStringEventType(aEventType);
}

/**
 * Convert relation type to human readable string.
 */
function relationTypeToString(aRelationType)
{
  return gAccRetrieval.getStringRelationType(aRelationType);
}

/**
 * Return pretty name for identifier, it may be ID, DOM node or accessible.
 */
function prettyName(aIdentifier)
{
  if (aIdentifier instanceof nsIAccessible) {
    var acc = getAccessible(aIdentifier, [nsIAccessNode]);
    return getNodePrettyName(acc.DOMNode) + ", role: " +
      roleToString(acc.finalRole);
  }

  if (aIdentifier instanceof nsIDOMNode)
    return getNodePrettyName(aIdentifier);

  return " '" + aIdentifier + "' ";
}

////////////////////////////////////////////////////////////////////////////////
// Private
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Accessible general

function initialize()
{
  gAccRetrieval = Components.classes["@mozilla.org/accessibleRetrieval;1"].
    getService(nsIAccessibleRetrieval);
}

addLoadEvent(initialize);

function getNodePrettyName(aNode)
{
  if (aNode.nodeType == nsIDOMNode.ELEMENT_NODE && aNode.hasAttribute("id"))
    return " '" + aNode.getAttribute("id") + "' ";

  return " '" + aNode.localName + " node' ";
}
