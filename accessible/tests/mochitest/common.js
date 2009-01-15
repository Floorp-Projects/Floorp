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

const ROLE_PUSHBUTTON = nsIAccessibleRole.ROLE_PUSHBUTTON;
const ROLE_COMBOBOX = nsIAccessibleRole.ROLE_COMBOBOX;
const ROLE_COMBOBOX_LIST = nsIAccessibleRole.ROLE_COMBOBOX_LIST;
const ROLE_COMBOBOX_OPTION = nsIAccessibleRole.ROLE_COMBOBOX_OPTION;
const ROLE_DOCUMENT = nsIAccessibleRole.ROLE_DOCUMENT;
const ROLE_ENTRY = nsIAccessibleRole.ROLE_ENTRY;
const ROLE_FLAT_EQUATION = nsIAccessibleRole.ROLE_FLAT_EQUATION;
const ROLE_LABEL = nsIAccessibleRole.ROLE_LABEL;
const ROLE_LIST = nsIAccessibleRole.ROLE_LIST;
const ROLE_OPTION = nsIAccessibleRole.ROLE_OPTION;
const ROLE_PARAGRAPH = nsIAccessibleRole.ROLE_PARAGRAPH;
const ROLE_PASSWORD_TEXT = nsIAccessibleRole.ROLE_PASSWORD_TEXT;
const ROLE_TEXT_CONTAINER = nsIAccessibleRole.ROLE_TEXT_CONTAINER;
const ROLE_TEXT_LEAF = nsIAccessibleRole.ROLE_TEXT_LEAF;
const ROLE_TOGGLE_BUTTON = nsIAccessibleRole.ROLE_TOGGLE_BUTTON;

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
const STATE_MULTISELECTABLE = nsIAccessibleStates.STATE_MULTISELECTABLE;
const STATE_PRESSED = nsIAccessibleStates.STATE_PRESSED;
const STATE_READONLY = nsIAccessibleStates.STATE_READONLY;
const STATE_SELECTABLE = nsIAccessibleStates.STATE_SELECTABLE;
const STATE_SELECTED = nsIAccessibleStates.STATE_SELECTED;

const EXT_STATE_EDITABLE = nsIAccessibleStates.EXT_STATE_EDITABLE;
const EXT_STATE_EXPANDABLE = nsIAccessibleStates.EXT_STATE_EXPANDABLE;
const EXT_STATE_INVALID = nsIAccessibleStates.STATE_INVALID;
const EXT_STATE_MULTI_LINE = nsIAccessibleStates.EXT_STATE_MULTI_LINE;
const EXT_STATE_REQUIRED = nsIAccessibleStates.STATE_REQUIRED;
const EXT_STATE_SUPPORTS_AUTOCOMPLETION = 
      nsIAccessibleStates.EXT_STATE_SUPPORTS_AUTOCOMPLETION;

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
 * Return accessible for the given ID attribute or DOM element or accessible.
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
  listenA11yEvents(true);

  gA11yEventApplicantsCount++;
  addA11yEventListener(aEventType, aEventHandler);
}

/**
 * Unregister accessibility event listener. Must be called for every registered
 * event listener (see registerA11yEventListener() function) when the listener
 * is not needed.
 */
function unregisterA11yEventListener(aEventType, aEventHandler)
{
  removeA11yEventListener(aEventType, aEventHandler);

  gA11yEventApplicantsCount--;
  listenA11yEvents(false);
}

/**
 * Creates event queue for the given event type. The queue consists of invoker
 * objects, each of them generates the event of the event type. When queue is
 * started then every invoker object is asked to generate event after timeout.
 * When event is caught then current invoker object is asked to check wether
 * event was handled correctly.
 *
 * Invoker interface is:
 *
 *   var invoker = {
 *     // Generates accessible event or event sequence.
 *     invoke: function(){},
 *
 *     // Invoker's check of handled event for correctness [optional].
 *     check: function(aEvent){},
 *
 *     // Is called when event of registered type is handled.
 *     debugCheck: function(aEvent){},
 *
 *     // DOM node event is generated for (the case when invoker generates
 *     // single event, see 'eventSeq' property).
 *     DOMNode getter() {},
 *
 *     // Array of items defining handled events. Every item is array with two
 *     // elements, first element is event type, second element is event target
 *     // (DOM node).
 *     eventSeq getter() {},
 *
 *     // The ID of invoker.
 *     getID: function(){} // returns invoker ID
 *   };
 *
 * @param  aEventType     [optional] the default event type (isn't used if
 *                        invoker defines eventSeq property).
 */
function eventQueue(aEventType)
{
  // public

  /**
   * Add invoker object into queue.
   */
  this.push = function eventQueue_push(aEventInvoker)
  {
    this.mInvokers.push(aEventInvoker);
  }

  /**
   * Start the queue processing.
   */
  this.invoke = function eventQueue_invoke()
  {
    listenA11yEvents(true);
    gA11yEventApplicantsCount++;

    window.setTimeout(function(aQueue) { aQueue.processInvoker(); }, 200, this);
  }

  // private

  this.processInvoker = function eventQueue_processInvoker()
  {
    this.clearEventHandler();
      
    if (this.mIndex == this.mInvokers.length - 1) {

      gA11yEventApplicantsCount--;
      listenA11yEvents(false);

      for (var idx = 0; idx < this.mInvokers.length; idx++) {
        var invoker = this.mInvokers[idx];
        var id = invoker.getID();

        if (invoker.wasCaught) {
          for (var jdx = 0; jdx < invoker.wasCaught.length; jdx++) {
            var seq = this.getEventSequence(invoker);
            var type = seq[jdx][0];
            var typeStr = gAccRetrieval.getStringEventType(type);

            var msg = "test with ID = '" + id + "' failed. ";
            if (invoker.doNotExpectEvents) {
              ok(!invoker.wasCaught[jdx],
                 msg + "There is unexpected " + typeStr + " event.");
            } else {
              ok(invoker.wasCaught[jdx],
                 msg + "No " + typeStr + " event.");
            }
          }
        } else {
          ok(false,
             "test with ID = '" + id + "' failed. No events were registered.");
        }
      }

      SimpleTest.finish();
      return;
    }

    this.mIndex++;
    this.setEventHandler();

    this.mInvokers[this.mIndex].invoke();

    window.setTimeout(function(aQueue) { aQueue.processInvoker(); }, 200, this);
  }

  this.getInvoker = function eventQueue_getInvoker()
  {
    return this.mInvokers[this.mIndex];
  }

  this.getEventSequence = function eventQueue_getEventSeq(aInvoker)
  {
    if (!aInvoker) // no invoker, return cached event sequence
      return this.mEventSeq;

    return ("eventSeq" in aInvoker) ?
      aInvoker.eventSeq : [[this.mDefEventType, aInvoker.DOMNode]];
  }

  this.setEventHandler = function eventQueue_setEventHandler()
  {
    var invoker = this.getInvoker();
    this.mEventSeq = this.getEventSequence(invoker);

    if (this.mEventSeq) {
      invoker.wasCaught = new Array(this.mEventSeq.length);

      for (var idx = 0; idx < this.mEventSeq.length; idx++)
        addA11yEventListener(this.mEventSeq[idx][0], this.mEventHandler);
    }
  }

  this.clearEventHandler = function eventQueue_clearEventHandler()
  {
    if (this.mEventSeq) {
      for (var idx = 0; idx < this.mEventSeq.length; idx++)
        removeA11yEventListener(this.mEventSeq[idx][0], this.mEventHandler);

      this.mEventSeq = null;
    }
  }

  this.mDefEventType = aEventType;
  this.mEventHandler = new eventHandlerForEventQueue(this);

  this.mInvokers = new Array();
  this.mIndex = -1;

  this.mEventSeq = null;
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
var gA11yEventApplicantsCount = 0;
var gA11yEventDumpID = ""; // set up this variable to dump events into DOM.

var gA11yEventObserver =
{
  observe: function observe(aSubject, aTopic, aData)
  {
    if (aTopic != "accessible-event")
      return;

    var event = aSubject.QueryInterface(nsIAccessibleEvent);
    var listenersArray = gA11yEventListeners[event.eventType];

    if (gA11yEventDumpID) { // debug stuff
      var type = gAccRetrieval.getStringEventType(event.eventType);
      var target = event.DOMNode;

      var info = "event type: " + type + ", target: " + target.localName;
      if (listenersArray)
        info += ", registered listeners count is " + listenersArray.length;

      var div = document.createElement("div");      
      div.textContent = info;

      var dumpElm = document.getElementById(gA11yEventDumpID);
      dumpElm.appendChild(div);
    }

    if (!listenersArray)
      return;

    for (var index = 0; index < listenersArray.length; index++)
      listenersArray[index].handleEvent(event);
  }
};

function listenA11yEvents(aStartToListen)
{
  if (aStartToListen && !gObserverService) {
    gObserverService = Components.classes["@mozilla.org/observer-service;1"].
      getService(nsIObserverService);
    
    gObserverService.addObserver(gA11yEventObserver, "accessible-event",
                                 false);
  } else if (!gA11yEventApplicantsCount) {
    gObserverService.removeObserver(gA11yEventObserver,
                                    "accessible-event");
    gObserverService = null;
  }
}

function addA11yEventListener(aEventType, aEventHandler)
{
  if (!(aEventType in gA11yEventListeners))
    gA11yEventListeners[aEventType] = new Array();
  
  gA11yEventListeners[aEventType].push(aEventHandler);
}

function removeA11yEventListener(aEventType, aEventHandler)
{
  var listenersArray = gA11yEventListeners[aEventType];
  if (!listenersArray)
    return false;

  var index = listenersArray.indexOf(aEventHandler);
  if (index == -1)
    return false;

  listenersArray.splice(index, 1);
  
  if (!listenersArray.length) {
    gA11yEventListeners[aEventType] = null;
    delete gA11yEventListeners[aEventType];
  }

  return true;
}

function eventHandlerForEventQueue(aQueue)
{
  this.handleEvent = function eventHandlerForEventQueue_handleEvent(aEvent)
  {
    var invoker = this.mQueue.getInvoker();
    var eventSeq = this.mQueue.getEventSequence();

    if (!invoker && !eventSeq) // skip events before test was started
      return;

    if (eventSeq != this.mEventSeq) {
      this.mEventSeqIdx = -1;
      this.mEventSeq = eventSeq;
    }

    if ("debugCheck" in invoker)
      invoker.debugCheck(aEvent);

    if (invoker.doNotExpectEvents) {
      // Search through event sequence to ensure it doesn't containt handled
      // event.
      for (var idx = 0; idx < this.mEventSeq.length; idx++) {
        if (aEvent.eventType == eventSeq[idx][0] &&
            aEvent.DOMNode == eventSeq[idx][1]) {
          invoker.wasCaught[idx] = true;
        }
      }
    } else {
      // We wait for events in order specified by evenSeq variable.
      var idx = this.mEventSeqIdx + 1;

      if (aEvent.eventType == eventSeq[idx][0] &&
          aEvent.DOMNode == eventSeq[idx][1]) {
        if ("check" in invoker)
          invoker.check(aEvent);

        invoker.wasCaught[idx] = true;
        this.mEventSeqIdx++;
      }
    }
  }

  this.mQueue = aQueue;
  this.mEventSeq = null;
  this.mEventSeqIdx = -1;
}
