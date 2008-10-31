////////////////////////////////////////////////////////////////////////////////
// Interfaces

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;

const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessibleStates = Components.interfaces.nsIAccessibleStates;
const nsIAccessibleRole = Components.interfaces.nsIAccessibleRole;
const nsIAccessibleTypes = Components.interfaces.nsIAccessibleTypes;

const nsIAccessibleRelation = Components.interfaces.nsIAccessibleRelation;

const nsIAccessNode = Components.interfaces.nsIAccessNode;
const nsIAccessible = Components.interfaces.nsIAccessible;

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

const nsIDOMNode = Components.interfaces.nsIDOMNode;
const nsIPropertyElement = Components.interfaces.nsIPropertyElement;

////////////////////////////////////////////////////////////////////////////////
// Roles

const ROLE_COMBOBOX = nsIAccessibleRole.ROLE_COMBOBOX;
const ROLE_COMBOBOX_LIST = nsIAccessibleRole.ROLE_COMBOBOX_LIST;
const ROLE_COMBOBOX_OPTION = nsIAccessibleRole.ROLE_COMBOBOX_OPTION;
const ROLE_DOCUMENT = nsIAccessibleRole.ROLE_DOCUMENT;
const ROLE_FLAT_EQUATION = nsIAccessibleRole.ROLE_FLAT_EQUATION;
const ROLE_LABEL = nsIAccessibleRole.ROLE_LABEL;
const ROLE_LIST = nsIAccessibleRole.ROLE_LIST;
const ROLE_OPTION = nsIAccessibleRole.ROLE_OPTION;
const ROLE_TEXT_LEAF = nsIAccessibleRole.ROLE_TEXT_LEAF;

////////////////////////////////////////////////////////////////////////////////
// States

const STATE_COLLAPSED = nsIAccessibleStates.STATE_COLLAPSED;
const STATE_EXPANDED = nsIAccessibleStates.STATE_EXPANDED;
const STATE_EXTSELECTABLE = nsIAccessibleStates.STATE_EXTSELECTABLE;
const STATE_FOCUSABLE = nsIAccessibleStates.STATE_FOCUSABLE;
const STATE_FOCUSED = nsIAccessibleStates.STATE_FOCUSED;
const STATE_HASPOPUP = nsIAccessibleStates.STATE_HASPOPUP;
const STATE_MULTISELECTABLE = nsIAccessibleStates.STATE_MULTISELECTABLE;
const STATE_READONLY = nsIAccessibleStates.STATE_READONLY;
const STATE_SELECTABLE = nsIAccessibleStates.STATE_SELECTABLE;
const STATE_SELECTED = nsIAccessibleStates.STATE_SELECTED;

const EXT_STATE_EDITABLE = nsIAccessibleStates.EXT_STATE_EDITABLE;
const EXT_STATE_EXPANDABLE = nsIAccessibleStates.EXT_STATE_EXPANDABLE;

////////////////////////////////////////////////////////////////////////////////
// Accessible general

/**
 * nsIAccessibleRetrieval, initialized when test is loaded.
 */
var gAccRetrieval = null;

/**
 * Return accessible for the given ID attribute or DOM element.
 *
 * @param aAccOrElmOrID  [in] DOM element or ID attribute to get an accessible
 *                        for or an accessible to query additional interfaces.
 * @param aInterfaces    [in, optional] the accessible interface or the array of
 *                        accessible interfaces to query it/them from obtained
 *                        accessible
 * @param aElmObj        [out, optional] object to store DOM element which
 *                        accessible is obtained for
 */
function getAccessible(aAccOrElmOrID, aInterfaces, aElmObj)
{
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

////////////////////////////////////////////////////////////////////////////////
// Accessible Events

/**
 * Register accessibility event listener.
 *
 * @param aEventType     the accessible event type (see nsIAccessibleEvent for
 *                       available constants).
 * @param aEventHandler  event listener object, when accessible event of the
 *                       given type is handled then 'handleEvent' method of
 *                       this object is invoked with nsIAccessibleEvent object
 *                       as the first argument.
 */
function registerA11yEventListener(aEventType, aEventHandler)
{
  if (!gA11yEventListenersCount) {
    gObserverService = Components.classes["@mozilla.org/observer-service;1"].
      getService(nsIObserverService);

    gObserverService.addObserver(gA11yEventObserver, "accessible-event",
                                 false);
  }

  if (!(aEventType in gA11yEventListeners))
    gA11yEventListeners[aEventType] = new Array();

  gA11yEventListeners[aEventType].push(aEventHandler);
  gA11yEventListenersCount++;
}

/**
 * Unregister accessibility event listener. Must be called for every registered
 * event listener (see registerA11yEventListener() function) when it's not
 * needed.
 */
function unregisterA11yEventListener(aEventType, aEventHandler)
{
  var listenersArray = gA11yEventListeners[aEventType];
  if (listenersArray) {
    var index = listenersArray.indexOf(aEventHandler);
    listenersArray.splice(index, 1);

    if (!listenersArray.length) {
      gA11yEventListeners[aEventType] = null;
      delete gA11yEventListeners[aEventType];
    }
  }
  
  gA11yEventListenersCount--;
  if (!gA11yEventListenersCount) {
    gObserverService.removeObserver(gA11yEventObserver,
                                    "accessible-event");
  }
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

////////////////////////////////////////////////////////////////////////////////
// Accessible Events

var gObserverService = null;

var gA11yEventListeners = {};
var gA11yEventListenersCount = 0;

var gA11yEventObserver =
{
  observe: function observe(aSubject, aTopic, aData)
  {
    if (aTopic != "accessible-event")
      return;

    var event = aSubject.QueryInterface(nsIAccessibleEvent);
    var listenersArray = gA11yEventListeners[event.eventType];
    if (!listenersArray)
      return;

    for (var index = 0; index < listenersArray.length; index++)
      listenersArray[index].handleEvent(event);
  }
};
