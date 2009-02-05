////////////////////////////////////////////////////////////////////////////////
// General

/**
 * Set up this variable to dump events into DOM.
 */
var gA11yEventDumpID = "";

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

////////////////////////////////////////////////////////////////////////////////
// Event queue

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
 *     // Array of items defining events expected (or not expected, see
 *     // 'doNotExpectEvents' property) on invoker's action. Every item is array
 *     // with two elements, first element is event type, second element is
 *     // event target (DOM node).
 *     eventSeq getter() {},
 *
 *     // Boolean indicates if events specified by 'eventSeq' property shouldn't
 *     // be triggerd by invoker.
 *     doNotExpectEvents getter() {},
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

    // XXX: Intermittent test_events_caretmove.html fails withouth timeout,
    // see bug 474952.
    window.setTimeout(function(aQueue) { aQueue.processNextInvoker(); }, 500,
                      this);
  }

  // private

  /**
   * Process next invoker.
   */
  this.processNextInvoker = function eventQueue_processNextInvoker()
  {
    // Finish rocessing of the current invoker.
    var testFailed = false;

    var invoker = this.getInvoker();
    if (invoker) {
      var id = invoker.getID();

      if (invoker.wasCaught) {
        for (var jdx = 0; jdx < invoker.wasCaught.length; jdx++) {
          var seq = this.mEventSeq;
          var type = seq[jdx][0];
          var typeStr = gAccRetrieval.getStringEventType(type);

          var msg = "test with ID = '" + id + "' failed. ";
          if (invoker.doNotExpectEvents) {
            var wasCaught = invoker.wasCaught[jdx];
            if (!testFailed)
              testFailed = wasCaught;

            ok(!wasCaught,
               msg + "There is unexpected " + typeStr + " event.");

          } else {
            var wasCaught = invoker.wasCaught[jdx];
            if (!testFailed)
              testFailed = !wasCaught;

            ok(wasCaught,
               msg + "No " + typeStr + " event.");
          }
        }
      } else {
        testFailed = true;
        ok(false,
           "test with ID = '" + id + "' failed. No events were registered.");
      }
    }

    this.clearEventHandler();

    // Check if need to stop the test.
    if (testFailed || this.mIndex == this.mInvokers.length - 1) {
      gA11yEventApplicantsCount--;
      listenA11yEvents(false);

      SimpleTest.finish();
      return;
    }

    // Start processing of next invoker.
    invoker = this.getNextInvoker();

    this.setEventHandler(invoker);

    invoker.invoke();

    if (invoker.doNotExpectEvents) {
      // Check in timeout invoker didn't fire registered events.
      window.setTimeout(function(aQueue) { aQueue.processNextInvoker(); }, 500,
                        this);
    }
  }

  /**
   * Handle events for the current invoker.
   */
  this.handleEvent = function eventQueue_handleEvent(aEvent)
  {
    var invoker = this.getInvoker();
    if (!invoker) // skip events before test was started
      return;

    if (!this.mEventSeq) {
      // Bad invoker object, error will be reported before processing of next
      // invoker in the queue.
      this.processNextInvoker();
      return;
    }

    if ("debugCheck" in invoker)
      invoker.debugCheck(aEvent);

    if (invoker.doNotExpectEvents) {
      // Search through event sequence to ensure it doesn't contain handled
      // event.
      for (var idx = 0; idx < this.mEventSeq.length; idx++) {
        if (aEvent.eventType == this.mEventSeq[idx][0] &&
            aEvent.DOMNode == this.mEventSeq[idx][1]) {
          invoker.wasCaught[idx] = true;
        }
      }
    } else {
      // We wait for events in order specified by eventSeq variable.
      var idx = this.mEventSeqIdx + 1;

      if (gA11yEventDumpID) { // debug stuff
        var eventType = this.mEventSeq[idx][0];
        var target = this.mEventSeq[idx][1];

        var info = "Event queue processing. Event type: ";
        info += gAccRetrieval.getStringEventType(eventType) + ". Target: ";
        info += (target.localName ? target.localName : target);
        if (target.nodeType == nsIDOMNode.ELEMENT_NODE &&
            target.hasAttribute("id"))
          info += " '" + target.getAttribute("id") + "'";

        dumpInfoToDOM(info);
      }

      if (aEvent.eventType == this.mEventSeq[idx][0] &&
          aEvent.DOMNode == this.mEventSeq[idx][1]) {

        if ("check" in invoker)
          invoker.check(aEvent);

        invoker.wasCaught[idx] = true;

        if (idx == this.mEventSeq.length - 1) {
          // We need delay to avoid events coalesce from different invokers.
          var queue = this;
          SimpleTest.executeSoon(function() { queue.processNextInvoker(); });
          return;
        }

        this.mEventSeqIdx = idx;
      }
    }
  }

  // Helpers
  this.getInvoker = function eventQueue_getInvoker()
  {
    return this.mInvokers[this.mIndex];
  }

  this.getNextInvoker = function eventQueue_getNextInvoker()
  {
    return this.mInvokers[++this.mIndex];
  }

  this.setEventHandler = function eventQueue_setEventHandler(aInvoker)
  {
    this.mEventSeq = ("eventSeq" in aInvoker) ?
      aInvoker.eventSeq : [[this.mDefEventType, aInvoker.DOMNode]];
    this.mEventSeqIdx = -1;

    if (this.mEventSeq) {
      aInvoker.wasCaught = new Array(this.mEventSeq.length);

      for (var idx = 0; idx < this.mEventSeq.length; idx++)
        addA11yEventListener(this.mEventSeq[idx][0], this);
    }
  }

  this.clearEventHandler = function eventQueue_clearEventHandler()
  {
    if (this.mEventSeq) {
      for (var idx = 0; idx < this.mEventSeq.length; idx++)
        removeA11yEventListener(this.mEventSeq[idx][0], this);
      
      this.mEventSeq = null;
    }
  }

  this.mDefEventType = aEventType;

  this.mInvokers = new Array();
  this.mIndex = -1;

  this.mEventSeq = null;
  this.mEventSeqIdx = -1;
}

////////////////////////////////////////////////////////////////////////////////
// Private implementation details.
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// General

var gObserverService = null;

var gA11yEventListeners = {};
var gA11yEventApplicantsCount = 0;

var gA11yEventObserver =
{
  observe: function observe(aSubject, aTopic, aData)
  {
    if (aTopic != "accessible-event")
      return;

    var event = aSubject.QueryInterface(nsIAccessibleEvent);
    var listenersArray = gA11yEventListeners[event.eventType];

    if (gA11yEventDumpID) { // debug stuff
      var target = event.DOMNode;
      var dumpElm = document.getElementById(gA11yEventDumpID);

      var parent = target;
      while (parent && parent != dumpElm)
        parent = parent.parentNode;

      if (parent != dumpElm) {
        var type = gAccRetrieval.getStringEventType(event.eventType);
        var info = "Event type: " + type + ". Target: ";
        info += (target.localName ? target.localName : target);

        if (target.nodeType == nsIDOMNode.ELEMENT_NODE &&
            target.hasAttribute("id"))
          info += " '" + target.getAttribute("id") + "'";

        if (listenersArray)
          info += ". Listeners count: " + listenersArray.length;

        dumpInfoToDOM(info);
      }
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

function dumpInfoToDOM(aInfo)
{
  var dumpElm = document.getElementById(gA11yEventDumpID);
  var div = document.createElement("div");      
  div.textContent = aInfo;
  dumpElm.appendChild(div);
}
