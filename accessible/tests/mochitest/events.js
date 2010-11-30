////////////////////////////////////////////////////////////////////////////////
// Constants

const EVENT_ALERT = nsIAccessibleEvent.EVENT_ALERT;
const EVENT_DOCUMENT_LOAD_COMPLETE = nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE;
const EVENT_DOCUMENT_RELOAD = nsIAccessibleEvent.EVENT_DOCUMENT_RELOAD;
const EVENT_DOCUMENT_LOAD_STOPPED = nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_STOPPED;
const EVENT_HIDE = nsIAccessibleEvent.EVENT_HIDE;
const EVENT_FOCUS = nsIAccessibleEvent.EVENT_FOCUS;
const EVENT_NAME_CHANGE = nsIAccessibleEvent.EVENT_NAME_CHANGE;
const EVENT_MENUPOPUP_START = nsIAccessibleEvent.EVENT_MENUPOPUP_START;
const EVENT_MENUPOPUP_END = nsIAccessibleEvent.EVENT_MENUPOPUP_END;
const EVENT_REORDER = nsIAccessibleEvent.EVENT_REORDER;
const EVENT_SCROLLING_START = nsIAccessibleEvent.EVENT_SCROLLING_START;
const EVENT_SELECTION_ADD = nsIAccessibleEvent.EVENT_SELECTION_ADD;
const EVENT_SELECTION_WITHIN = nsIAccessibleEvent.EVENT_SELECTION_WITHIN;
const EVENT_SHOW = nsIAccessibleEvent.EVENT_SHOW;
const EVENT_STATE_CHANGE = nsIAccessibleEvent.EVENT_STATE_CHANGE;
const EVENT_TEXT_ATTRIBUTE_CHANGED = nsIAccessibleEvent.EVENT_TEXT_ATTRIBUTE_CHANGED;
const EVENT_TEXT_CARET_MOVED = nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED;
const EVENT_TEXT_INSERTED = nsIAccessibleEvent.EVENT_TEXT_INSERTED;
const EVENT_TEXT_REMOVED = nsIAccessibleEvent.EVENT_TEXT_REMOVED;
const EVENT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_VALUE_CHANGE;

////////////////////////////////////////////////////////////////////////////////
// General

/**
 * Set up this variable to dump events into DOM.
 */
var gA11yEventDumpID = "";

/**
 * Set up this variable to dump event processing into console.
 */
var gA11yEventDumpToConsole = false;

/**
 * Executes the function when requested event is handled.
 *
 * @param aEventType  [in] event type
 * @param aTarget     [in] event target
 * @param aFunc       [in] function to call when event is handled
 * @param aContext    [in, optional] object in which context the function is
 *                    called
 * @param aArg1       [in, optional] argument passed into the function
 * @param aArg2       [in, optional] argument passed into the function
 */
function waitForEvent(aEventType, aTarget, aFunc, aContext, aArg1, aArg2)
{
  var handler = {
    handleEvent: function handleEvent(aEvent) {

      if (aTarget) {
        if (aTarget instanceof nsIAccessible &&
            aTarget != aEvent.accessible)
          return;

        if (aTarget instanceof nsIDOMNode &&
            aTarget != aEvent.DOMNode)
          return;
      }

      unregisterA11yEventListener(aEventType, this);

      window.setTimeout(
        function ()
        {
          aFunc.call(aContext, aArg1, aArg2);
        },
        0
      );
    }
  };

  registerA11yEventListener(aEventType, handler);
}

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
  listenA11yEvents(false);
}


////////////////////////////////////////////////////////////////////////////////
// Event queue

/**
 * Return value of invoke method of invoker object. Indicates invoker was unable
 * to prepare action.
 */
const INVOKER_ACTION_FAILED = 1;

/**
 * Return value of eventQueue.onFinish. Indicates eventQueue should not finish
 * tests.
 */
const DO_NOT_FINISH_TEST = 1;

/**
 * Creates event queue for the given event type. The queue consists of invoker
 * objects, each of them generates the event of the event type. When queue is
 * started then every invoker object is asked to generate event after timeout.
 * When event is caught then current invoker object is asked to check whether
 * event was handled correctly.
 *
 * Invoker interface is:
 *
 *   var invoker = {
 *     // Generates accessible event or event sequence. If returns
 *     // INVOKER_ACTION_FAILED constant then stop tests.
 *     invoke: function(){},
 *
 *     // [optional] Invoker's check of handled event for correctness.
 *     check: function(aEvent){},
 *
 *     // [optional] Invoker's check before the next invoker is proceeded.
 *     finalCheck: function(aEvent){},
 *
 *     // [optional] Is called when event of registered type is handled.
 *     debugCheck: function(aEvent){},
 *
 *     // [ignored if 'eventSeq' is defined] DOM node event is generated for
 *     // (used in the case when invoker expects single event).
 *     DOMNode getter: function() {},
 *
 *     // Array of checker objects defining expected events on invoker's action.
 *     //
 *     // Checker object interface:
 *     //
 *     // var checker = {
 *     //   type getter: function() {}, // DOM or a11y event type
 *     //   target getter: function() {}, // DOM node or accessible
 *     //   phase getter: function() {}, // DOM event phase (false - bubbling)
 *     //   check: function(aEvent) {},
 *     //   getID: function() {}
 *     // };
 *     eventSeq getter() {},
 *
 *     // Array of checker objects defining unexpected events on invoker's
 *     // action.
 *     unexpectedEventSeq getter() {},
 *
 *     // The ID of invoker.
 *     getID: function(){} // returns invoker ID
 *   };
 *
 * @param  aEventType  [in, optional] the default event type (isn't used if
 *                      invoker defines eventSeq property).
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

    // XXX: Intermittent test_events_caretmove.html fails withouth timeout,
    // see bug 474952.
    this.processNextInvokerInTimeout(true);
  }

  /**
   * This function is called when all events in the queue were handled.
   * Override it if you need to be notified of this.
   */
  this.onFinish = function eventQueue_finish()
  {
  }

  // private

  /**
   * Process next invoker.
   */
  this.processNextInvoker = function eventQueue_processNextInvoker()
  {
    // Finish processing of the current invoker.
    var testFailed = false;

    var invoker = this.getInvoker();
    if (invoker) {
      if ("finalCheck" in invoker)
        invoker.finalCheck();

      if (invoker.wasCaught) {
        for (var idx = 0; idx < invoker.wasCaught.length; idx++) {
          var id = this.getEventID(idx);
          var type = this.getEventType(idx);
          var unexpected = this.mEventSeq[idx].unexpected;

          var typeStr = (typeof type == "string") ?
            type : gAccRetrieval.getStringEventType(type);

          var msg = "test with ID = '" + id + "' failed. ";
          if (unexpected) {
            var wasCaught = invoker.wasCaught[idx];
            if (!testFailed)
              testFailed = wasCaught;

            ok(!wasCaught,
               msg + "There is unexpected " + typeStr + " event.");

          } else {
            var wasCaught = invoker.wasCaught[idx];
            if (!testFailed)
              testFailed = !wasCaught;

            ok(wasCaught,
               msg + "No " + typeStr + " event.");
          }
        }
      } else {
        testFailed = true;
        for (var idx = 0; idx < this.mEventSeq.length; idx++) {
          var id = this.getEventID(idx);
          ok(false,
             "test with ID = '" + id + "' failed. No events were registered.");
        }
      }
    }

    this.clearEventHandler();

    // Check if need to stop the test.
    if (testFailed || this.mIndex == this.mInvokers.length - 1) {
      listenA11yEvents(false);

      var res = this.onFinish();
      if (res != DO_NOT_FINISH_TEST)
        SimpleTest.finish();

      return;
    }

    // Start processing of next invoker.
    invoker = this.getNextInvoker();

    this.setEventHandler(invoker);

    if (gA11yEventDumpToConsole)
      dump("\nEvent queue: \n  invoke: " + invoker.getID() + "\n");

    if (invoker.invoke() == INVOKER_ACTION_FAILED) {
      // Invoker failed to prepare action, fail and finish tests.
      this.processNextInvoker();
      return;
    }

    if (this.areAllEventsUnexpected())
      this.processNextInvokerInTimeout(true);
  }

  this.processNextInvokerInTimeout = function eventQueue_processNextInvokerInTimeout(aUncondProcess)
  {
    if (!aUncondProcess && this.areAllEventsExpected()) {
      // We need delay to avoid events coalesce from different invokers.
      var queue = this;
      SimpleTest.executeSoon(function() { queue.processNextInvoker(); });
      return;
    }

    // Check in timeout invoker didn't fire registered events.
    window.setTimeout(function(aQueue) { aQueue.processNextInvoker(); }, 500,
                      this);
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

    // Search through unexpected events to ensure no one of them was handled.
    var idx = 0;
    for (; idx < this.mEventSeq.length; idx++) {
      if (this.mEventSeq[idx].unexpected && this.compareEvents(idx, aEvent))
        invoker.wasCaught[idx] = true;
    }

    // We've handled all expected events, next invoker processing is pending.
    if (this.mEventSeqIdx == this.mEventSeq.length)
      return;

    // Compute next expected event index.
    for (idx = this.mEventSeqIdx + 1;
         idx < this.mEventSeq.length && this.mEventSeq[idx].unexpected;
         idx++);

    // No expected events were registered, proceed to next invoker to ensure
    // unexpected events for current invoker won't be handled.
    if (idx == this.mEventSeq.length) {
      this.mEventSeqIdx = idx;
      this.processNextInvokerInTimeout();
      return;
    }

    // Check if handled event matches expected event.
    var matched = this.compareEvents(idx, aEvent);
    this.dumpEventToDOM(aEvent, idx, matched);

    if (matched) {
      this.checkEvent(idx, aEvent);
      invoker.wasCaught[idx] = true;
      this.mEventSeqIdx = idx;

      // Get next expected event index.
      while (++idx < this.mEventSeq.length && this.mEventSeq[idx].unexpected);

      // If the last expected event was processed, proceed next invoker in
      // timeout to ensure unexpected events for current invoker won't be
      // handled.
      if (idx == this.mEventSeq.length) {
        this.mEventSeqIdx = idx;
        this.processNextInvokerInTimeout();
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
    // Create unique event sequence concatenating expected and unexpected
    // events.
    this.mEventSeq = ("eventSeq" in aInvoker) ?
      aInvoker.eventSeq :
      [ new invokerChecker(this.mDefEventType, aInvoker.DOMNode) ];

    for (var idx = 0; idx < this.mEventSeq.length; idx++)
      this.mEventSeq[idx].unexpected = false;

    var unexpectedSeq = aInvoker.unexpectedEventSeq;
    if (unexpectedSeq) {
      for (var idx = 0; idx < unexpectedSeq.length; idx++)
        unexpectedSeq[idx].unexpected = true;

      this.mEventSeq = this.mEventSeq.concat(unexpectedSeq);
    }

    this.mEventSeqIdx = -1;

    // Register event listeners
    if (this.mEventSeq) {
      aInvoker.wasCaught = new Array(this.mEventSeq.length);

      for (var idx = 0; idx < this.mEventSeq.length; idx++) {
        var eventType = this.getEventType(idx);
        if (typeof eventType == "string") {
          // DOM event
          var target = this.getEventTarget(idx);
          var phase = this.getEventPhase(idx);
          target.ownerDocument.addEventListener(eventType, this, phase);

        } else {
          // A11y event
          addA11yEventListener(eventType, this);
        }
      }
    }
  }

  this.clearEventHandler = function eventQueue_clearEventHandler()
  {
    if (this.mEventSeq) {
      for (var idx = 0; idx < this.mEventSeq.length; idx++) {
        var eventType = this.getEventType(idx);
        if (typeof eventType == "string") {
          // DOM event
          var target = this.getEventTarget(idx);
          var phase = this.getEventPhase(idx);
          target.ownerDocument.removeEventListener(eventType, this, phase);

        } else {
          // A11y event
          removeA11yEventListener(eventType, this);
        }
      }

      this.mEventSeq = null;
    }
  }

  this.getEventType = function eventQueue_getEventType(aIdx)
  {
    return this.mEventSeq[aIdx].type;
  }

  this.getEventTarget = function eventQueue_getEventTarget(aIdx)
  {
    return this.mEventSeq[aIdx].target;
  }

  this.getEventPhase = function eventQueue_getEventPhase(aIdx)
  {
     var eventItem = this.mEventSeq[aIdx];
    if ("phase" in eventItem)
      return eventItem.phase;

    return true;
  }

  this.getEventID = function eventQueue_getEventID(aIdx)
  {
    var eventItem = this.mEventSeq[aIdx];
    if ("getID" in eventItem)
      return eventItem.getID();
    
    var invoker = this.getInvoker();
    return invoker.getID();
  }

  this.compareEvents = function eventQueue_compareEvents(aIdx, aEvent)
  {
    var eventType1 = this.getEventType(aIdx);

    var eventType2 = (aEvent instanceof nsIDOMEvent) ?
      aEvent.type : aEvent.eventType;

    if (eventType1 != eventType2)
      return false;

    var target1 = this.getEventTarget(aIdx);
    if (target1 instanceof nsIAccessible) {
      var target2 = (aEvent instanceof nsIDOMEvent) ?
        getAccessible(aEvent.target) : aEvent.accessible;

      return target1 == target2;
    }

    // If original target isn't suitable then extend interface to support target
    // (original target is used in test_elm_media.html).
    var target2 = (aEvent instanceof nsIDOMEvent) ?
      aEvent.originalTarget : aEvent.DOMNode;
    return target1 == target2;
  }

  this.checkEvent = function eventQueue_checkEvent(aIdx, aEvent)
  {
    var eventItem = this.mEventSeq[aIdx];
    if ("check" in eventItem)
      eventItem.check(aEvent);

    var invoker = this.getInvoker();
    if ("check" in invoker)
      invoker.check(aEvent);
  }

  this.areAllEventsExpected = function eventQueue_areAllEventsExpected()
  {
    for (var idx = 0; idx < this.mEventSeq.length; idx++) {
      if (this.mEventSeq[idx].unexpected)
        return false;
    }

    return true;
  }

  this.areAllEventsUnexpected = function eventQueue_areAllEventsUnxpected()
  {
    for (var idx = 0; idx < this.mEventSeq.length; idx++) {
      if (!this.mEventSeq[idx].unexpected)
        return false;
    }

    return true;
  }

  this.dumpEventToDOM = function eventQueue_dumpEventToDOM(aOrigEvent,
                                                           aExpectedEventIdx,
                                                           aMatch)
  {
    if (!gA11yEventDumpID) // debug stuff
      return;

    // Dump DOM event information. Skip a11y event since it is dumped by
    // gA11yEventObserver.
    if (aOrigEvent instanceof nsIDOMEvent) {
      var info = "Event type: " + aOrigEvent.type;
      info += ". Target: " + prettyName(aOrigEvent.originalTarget);
      dumpInfoToDOM(info);
    }

    var currType = this.getEventType(aExpectedEventIdx);
    var currTarget = this.getEventTarget(aExpectedEventIdx);

    var containerTagName = document instanceof nsIDOMHTMLDocument ?
      "div" : "description";
    var inlineTagName = document instanceof nsIDOMHTMLDocument ?
      "span" : "description";

    var container = document.createElement(containerTagName);
    container.setAttribute("style", "padding-left: 10px;");

    var text1 = document.createTextNode("EQ: ");
    container.appendChild(text1);

    var styledNode = document.createElement(inlineTagName);
    if (aMatch) {
      styledNode.setAttribute("style", "color: blue;");
      styledNode.textContent = "matched";

      // Dump matched events into console.
      if (gA11yEventDumpToConsole)
        dump("\n*****\nEQ matched: " + eventTypeToString(currType) + "\n*****\n");

    } else {
      styledNode.textContent = "expected";
    }
    container.appendChild(styledNode);

    var info = " event, type: ";
    info += (typeof currType == "string") ?
      currType : eventTypeToString(currType);
    info += ". Target: " + prettyName(currTarget);

    var text1 = document.createTextNode(info);
    container.appendChild(text1);

    dumpInfoToDOM(container);
  }

  this.mDefEventType = aEventType;

  this.mInvokers = new Array();
  this.mIndex = -1;

  this.mEventSeq = null;
  this.mEventSeqIdx = -1;
}


////////////////////////////////////////////////////////////////////////////////
// Action sequence

/**
 * Deal with action sequence. Used when you need to execute couple of actions
 * each after other one.
 */
function sequence()
{
  /**
   * Append new sequence item.
   *
   * @param  aProcessor  [in] object implementing interface
   *                      {
   *                        // execute item action
   *                        process: function() {},
   *                        // callback, is called when item was processed
   *                        onProcessed: function() {}
   *                      };
   * @param  aEventType  [in] event type of expected event on item action
   * @param  aTarget     [in] event target of expected event on item action
   * @param  aItemID     [in] identifier of item
   */
  this.append = function sequence_append(aProcessor, aEventType, aTarget,
                                         aItemID)
  {
    var item = new sequenceItem(aProcessor, aEventType, aTarget, aItemID);
    this.items.push(item);
  }

  /**
   * Process next sequence item.
   */
  this.processNext = function sequence_processNext()
  {
    this.idx++;
    if (this.idx >= this.items.length) {
      ok(false, "End of sequence: nothing to process!");
      SimpleTest.finish();
      return;
    }

    this.items[this.idx].startProcess();
  }

  this.items = new Array();
  this.idx = -1;
}


////////////////////////////////////////////////////////////////////////////////
// Event queue invokers

/**
 * Invokers defined below take a checker object implementing 'check' method
 * which will be called when proper event is handled. Invokers listen default
 * event type registered in event queue object until it is passed explicetly.
 *
 * Note, checker object is optional.
 * Note, you don't need to initialize 'target' and 'type' members of checker
 * object. The 'target' member will be initialized by invoker object and you are
 * free to use it in 'check' method.
 */

/**
 * Click invoker.
 */
function synthClick(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthAction(aNodeOrID, aChecker, aEventType);

  this.invoke = function synthClick_invoke()
  {
    // Scroll the node into view, otherwise synth click may fail.
    if (this.DOMNode instanceof nsIDOMNSHTMLElement)
      this.DOMNode.scrollIntoView(true);

    synthesizeMouse(this.DOMNode, 1, 1, {});
  }

  this.getID = function synthClick_getID()
  {
    return prettyName(aNodeOrID) + " click"; 
  }
}

/**
 * Mouse move invoker.
 */
function synthMouseMove(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthAction(aNodeOrID, aChecker, aEventType);

  this.invoke = function synthMouseMove_invoke()
  {
    synthesizeMouse(this.DOMNode, 1, 1, { type: "mousemove" });
    synthesizeMouse(this.DOMNode, 2, 2, { type: "mousemove" });
  }

  this.getID = function synthMouseMove_getID()
  {
    return prettyName(aNodeOrID) + " mouse move"; 
  }
}

/**
 * General key press invoker.
 */
function synthKey(aNodeOrID, aKey, aArgs, aChecker, aEventType)
{
  this.__proto__ = new synthAction(aNodeOrID, aChecker, aEventType);

  this.invoke = function synthKey_invoke()
  {
    synthesizeKey(this.mKey, this.mArgs);
  }

  this.getID = function synthKey_getID()
  {
    return prettyName(aNodeOrID) + " '" + this.mKey + "' key"; 
  }

  this.mKey = aKey;
  this.mArgs = aArgs ? aArgs : {};
}

/**
 * Tab key invoker.
 */
function synthTab(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_TAB", { shiftKey: false },
                                aChecker, aEventType);

  this.getID = function synthTab_getID() 
  { 
    return prettyName(aNodeOrID) + " tab";
  }
}

/**
 * Shift tab key invoker.
 */
function synthShiftTab(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_TAB", { shiftKey: true },
                                aChecker, aEventType);

  this.getID = function synthTabTest_getID() 
  { 
    return prettyName(aNodeOrID) + " shift tab";
  }
}

/**
 * Down arrow key invoker.
 */
function synthDownKey(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_DOWN", null, aChecker,
                                aEventType);

  this.getID = function synthDownKey_getID()
  {
    return prettyName(aNodeOrID) + " key down";
  }
}

/**
 * Right arrow key invoker.
 */
function synthRightKey(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_RIGHT", null, aChecker,
                                aEventType);

  this.getID = function synthRightKey_getID()
  {
    return prettyName(aNodeOrID) + " key right";
  }
}

/**
 * Home key invoker.
 */
function synthHomeKey(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_HOME", null, aChecker,
                                aEventType);
  
  this.getID = function synthHomeKey_getID()
  {
    return prettyName(aNodeOrID) + " key home";
  }
}

/**
 * Focus invoker.
 */
function synthFocus(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthAction(aNodeOrID, aChecker, aEventType);

  this.invoke = function synthFocus_invoke()
  {
    this.DOMNode.focus();
  }

  this.getID = function synthFocus_getID() 
  { 
    return prettyName(aNodeOrID) + " focus";
  }
}

/**
 * Focus invoker. Focus the HTML body of content document of iframe.
 */
function synthFocusOnFrame(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthAction(getNode(aNodeOrID).contentDocument,
                                   aChecker, aEventType);
  
  this.invoke = function synthFocus_invoke()
  {
    this.DOMNode.body.focus();
  }
  
  this.getID = function synthFocus_getID() 
  { 
    return prettyName(aNodeOrID) + " frame document focus";
  }
}

/**
 * Select all invoker.
 */
function synthSelectAll(aNodeOrID, aChecker, aEventType)
{
  this.__proto__ = new synthAction(aNodeOrID, aChecker, aEventType);

  this.invoke = function synthSelectAll_invoke()
  {
    if (this.DOMNode instanceof Components.interfaces.nsIDOMHTMLInputElement)
      this.DOMNode.select();
    else
      window.getSelection().selectAllChildren(this.DOMNode);
  }

  this.getID = function synthSelectAll_getID()
  {
    return aNodeOrID + " selectall";
  }
}


////////////////////////////////////////////////////////////////////////////////
// Event queue checkers

/**
 * Common invoker checker (see eventSeq of eventQueue).
 */
function invokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg)
{
  this.type = aEventType;

  this.__defineGetter__("target", invokerChecker_targetGetter);
  this.__defineSetter__("target", invokerChecker_targetSetter);

  // implementation details
  function invokerChecker_targetGetter()
  {
    if (typeof this.mTarget == "function")
      return this.mTarget.call(null, this.mTargetFuncArg);

    return this.mTarget;
  }

  function invokerChecker_targetSetter(aValue)
  {
    this.mTarget = aValue;
    return this.mTarget;
  }

  this.mTarget = aTargetOrFunc;
  this.mTargetFuncArg = aTargetFuncArg;
}


////////////////////////////////////////////////////////////////////////////////
// Private implementation details.
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// General

var gA11yEventListeners = {};
var gA11yEventApplicantsCount = 0;

var gA11yEventObserver =
{
  // The service reference needs to live in the observer, instead of as a global var,
  //   to be available in observe() catch case too.
  observerService :
    Components.classes["@mozilla.org/observer-service;1"]
              .getService(nsIObserverService),

  observe: function observe(aSubject, aTopic, aData)
  {
    if (aTopic != "accessible-event")
      return;

    var event;
    try {
      event = aSubject.QueryInterface(nsIAccessibleEvent);
    } catch (ex) {
      // After a test is aborted (i.e. timed out by the harness), this exception is soon triggered.
      // Remove the leftover observer, otherwise it "leaks" to all the following tests.
      this.observerService.removeObserver(this, "accessible-event");
      // Forward the exception, with added explanation.
      throw "[accessible/events.js, gA11yEventObserver.observe] This is expected if a previous test has been aborted... Initial exception was: [ " + ex + " ]";
    }
    var listenersArray = gA11yEventListeners[event.eventType];

    var eventFromDumpArea = false;
    if (gA11yEventDumpID) { // debug stuff
      eventFromDumpArea = true;

      var target = event.DOMNode;
      var dumpElm = document.getElementById(gA11yEventDumpID);

      var parent = target;
      while (parent && parent != dumpElm)
        parent = parent.parentNode;

      if (parent != dumpElm) {
        var type = eventTypeToString(event.eventType);
        var info = "Event type: " + type;

        if (event instanceof nsIAccessibleTextChangeEvent) {
          info += ", start: " + event.start + ", length: " + event.length +
            ", " + (event.isInserted() ? "inserted" : "removed") +
            " text: " + event.modifiedText;
        }

        info += ". Target: " + prettyName(event.accessible);

        if (listenersArray)
          info += ". Listeners count: " + listenersArray.length;

        eventFromDumpArea = false;

        if (gA11yEventDumpToConsole)
          dump("\n" + info + "\n");
        dumpInfoToDOM(info);
      }
    }

    // Do not notify listeners if event is result of event log changes.
    if (!listenersArray || eventFromDumpArea)
      return;

    for (var index = 0; index < listenersArray.length; index++)
      listenersArray[index].handleEvent(event);
  }
};

function listenA11yEvents(aStartToListen)
{
  if (aStartToListen) {
    // Add observer when adding the first applicant only.
    if (!(gA11yEventApplicantsCount++))
      gA11yEventObserver.observerService
                        .addObserver(gA11yEventObserver, "accessible-event", false);
  } else {
    // Remove observer when there are no more applicants only.
    // '< 0' case should not happen, but just in case: removeObserver() will throw.
    if (--gA11yEventApplicantsCount <= 0)
      gA11yEventObserver.observerService
                        .removeObserver(gA11yEventObserver, "accessible-event");
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

/**
 * Dumps message to DOM.
 *
 * @param aInfo      [in] the message or DOM node to dump
 * @param aDumpNode  [in, optional] host DOM node for dumped message, if ommited
 *                    then global variable gA11yEventDumpID is used
 */
function dumpInfoToDOM(aInfo, aDumpNode)
{
  var dumpID = gA11yEventDumpID ? gA11yEventDumpID : aDumpNode;
  if (!dumpID)
    return;
  
  var dumpElm = document.getElementById(dumpID);
  if (!dumpElm) {
    ok(false, "No dump element '" + dumpID + "' within the document!");
    return;
  }
  
  var containerTagName = document instanceof nsIDOMHTMLDocument ?
    "div" : "description";

  var container = document.createElement(containerTagName);
  if (aInfo instanceof nsIDOMNode)
    container.appendChild(aInfo);
  else
    container.textContent = aInfo;

  dumpElm.appendChild(container);
}


////////////////////////////////////////////////////////////////////////////////
// Sequence

/**
 * Base class of sequence item.
 */
function sequenceItem(aProcessor, aEventType, aTarget, aItemID)
{
  // private
  
  this.startProcess = function sequenceItem_startProcess()
  {
    this.queue.invoke();
  }
  
  var item = this;
  
  this.queue = new eventQueue();
  this.queue.onFinish = function()
  {
    aProcessor.onProcessed();
    return DO_NOT_FINISH_TEST;
  }
  
  var invoker = {
    invoke: function invoker_invoke() {
      return aProcessor.process();
    },
    getID: function invoker_getID()
    {
      return aItemID;
    },
    eventSeq: [ new invokerChecker(aEventType, aTarget) ]
  };
  
  this.queue.push(invoker);
}

////////////////////////////////////////////////////////////////////////////////
// Event queue invokers

/**
 * Invoker base class for prepare an action.
 */
function synthAction(aNodeOrID, aChecker, aEventType)
{
  this.DOMNode = getNode(aNodeOrID);
  if (aChecker)
    aChecker.target = this.DOMNode;

  if (aEventType)
    this.eventSeq = [ new invokerChecker(aEventType, this.DOMNode) ];

  this.check = function synthAction_check(aEvent)
  {
    if (aChecker)
      aChecker.check(aEvent);
  }

  this.getID = function synthAction_getID() { return aNodeOrID + " action"; }
}
