////////////////////////////////////////////////////////////////////////////////
// Constants

const EVENT_ALERT = nsIAccessibleEvent.EVENT_ALERT;
const EVENT_DOCUMENT_LOAD_COMPLETE = nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE;
const EVENT_DOCUMENT_RELOAD = nsIAccessibleEvent.EVENT_DOCUMENT_RELOAD;
const EVENT_DOCUMENT_LOAD_STOPPED = nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_STOPPED;
const EVENT_HIDE = nsIAccessibleEvent.EVENT_HIDE;
const EVENT_FOCUS = nsIAccessibleEvent.EVENT_FOCUS;
const EVENT_NAME_CHANGE = nsIAccessibleEvent.EVENT_NAME_CHANGE;
const EVENT_MENU_START = nsIAccessibleEvent.EVENT_MENU_START;
const EVENT_MENU_END = nsIAccessibleEvent.EVENT_MENU_END;
const EVENT_MENUPOPUP_START = nsIAccessibleEvent.EVENT_MENUPOPUP_START;
const EVENT_MENUPOPUP_END = nsIAccessibleEvent.EVENT_MENUPOPUP_END;
const EVENT_OBJECT_ATTRIBUTE_CHANGED = nsIAccessibleEvent.EVENT_OBJECT_ATTRIBUTE_CHANGED;
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
 * Set up this variable to dump event processing into error console.
 */
var gA11yEventDumpToAppConsole = false;

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
 *     //   * DOM or a11y event type. *
 *     //   type getter: function() {},
 *     //
 *     //   * DOM node or accessible. *
 *     //   target getter: function() {},
 *     //
 *     //   * DOM event phase (false - bubbling). *
 *     //   phase getter: function() {},
 *     //
 *     //   * Callback, called to match handled event. *
 *     //   match : function() {},
 *     //
 *     //   * Callback, called when event is handled
 *     //   check: function(aEvent) {},
 *     //
 *     //   * Checker ID *
 *     //   getID: function() {},
 *     //
 *     //   * Event that don't have predefined order relative other events. *
 *     //   async getter: function() {},
 *     //
 *     //   * Event that is not expected. *
 *     //   unexpected getter: function() {},
 *     //
 *     //   * No other event of the same type is not allowed. *
 *     //   unique getter: function() {}
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

          var typeStr = this.getEventTypeAsString(idx);

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

    if (gLogger.isEnabled()) {
      gLogger.logToConsole("Event queue: \n  invoke: " + invoker.getID());
      gLogger.logToDOM("EQ: invoke: " + invoker.getID(), true);
    }

    this.setEventHandler(invoker);

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
    window.setTimeout(function(aQueue) { aQueue.processNextInvoker(); }, 100,
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

    // Search through handled expected events to report error if one of them is
    // handled for a second time.
    var idx = 0;
    for (; idx < this.mEventSeq.length; idx++) {
      if (this.isEventExpected(idx) && (invoker.wasCaught[idx] == true) &&
          this.isSameEvent(idx, aEvent)) {

        var msg = "Doubled event { event type: " +
          this.getEventTypeAsString(idx) + ", target: " +
          this.getEventTargetDescr(idx) + "} in test with ID = '" +
          this.getEventID(idx) + "'.";
        ok(false, msg);
      }
    }

    // Search through unexpected events, any matches result in error report
    // after this invoker processing.
    for (idx = 0; idx < this.mEventSeq.length; idx++) {
      if (this.isEventUnexpected(idx) && this.compareEvents(idx, aEvent))
        invoker.wasCaught[idx] = true;
    }

    // Nothing left, proceed next invoker in timeout. Otherwise check if
    // handled event is matched.
    var idxObj = {};
    if (!this.prepareForExpectedEvent(invoker, idxObj))
      return;

    // Check if handled event matches expected sync event.
    var matched = false;
    idx = idxObj.value;
    if (idx < this.mEventSeq.length) {
      matched = this.compareEvents(idx, aEvent);
      if (matched)
        this.mEventSeqIdx = idx;
    }

    // Check if handled event matches any expected async events.
    if (!matched) {
      for (idx = 0; idx < this.mEventSeq.length; idx++) {
        if (this.mEventSeq[idx].async) {
          matched = this.compareEvents(idx, aEvent);
          if (matched)
            break;
        }
      }
    }
    this.dumpEventToDOM(aEvent, idx, matched);

    if (matched) {
      this.checkEvent(idx, aEvent);
      invoker.wasCaught[idx] = true;

      this.prepareForExpectedEvent(invoker);
    }
  }

  // Helpers
  this.prepareForExpectedEvent =
    function eventQueue_prepareForExpectedEvent(aInvoker, aIdxObj)
  {
    // Nothing left, wait for next invoker.
    if (this.mEventSeqFinished)
      return false;

    // Compute next expected sync event index.
    for (var idx = this.mEventSeqIdx + 1;
         idx < this.mEventSeq.length &&
         (this.mEventSeq[idx].unexpected || this.mEventSeq[idx].async);
         idx++);

    // If no expected events were left, proceed to next invoker in timeout
    // to make sure unexpected events for current invoker aren't be handled.
    if (idx == this.mEventSeq.length) {
      var allHandled = true;
      for (var jdx = 0; jdx < this.mEventSeq.length; jdx++) {
        if (this.isEventExpected(jdx) && !aInvoker.wasCaught[jdx])
          allHandled = false;
      }

      if (allHandled) {
        this.mEventSeqIdx = this.mEventSeq.length;
        this.mEventFinished = true;
        this.processNextInvokerInTimeout();
        return false;
      }
    }

    if (aIdxObj)
      aIdxObj.value = idx;

    return true;
  }

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
    // Create unified event sequence concatenating expected and unexpected
    // events.
    this.mEventSeq = ("eventSeq" in aInvoker) ?
      aInvoker.eventSeq :
      [ new invokerChecker(this.mDefEventType, aInvoker.DOMNode) ];

    var len = this.mEventSeq.length;
    for (var idx = 0; idx < len; idx++) {
      var seqItem = this.mEventSeq[idx];
      // Allow unexpected events in primary event sequence.
      if (!("unexpected" in this.mEventSeq[idx]))
        seqItem.unexpected = false;

      if (!("async" in this.mEventSeq[idx]))
        seqItem.async = false;

      // If the event is of unique type (regardless whether it's expected or
      // not) then register additional unexpected event that matches to any
      // event of the same type with any target different from registered
      // expected events.
      if (("unique" in seqItem) && seqItem.unique) {
        var uniquenessChecker = {
          type: seqItem.type,
          unexpected: true,
          match: function uniquenessChecker_match(aEvent)
          {
            // The handled event is matched if its target doesn't match to any
            // registered expected event.
            var matched = true;
            for (var idx = 0; idx < this.queue.mEventSeq.length; idx++) {
              if (this.queue.isEventExpected(idx) &&
                  this.queue.compareEvents(idx, aEvent)) {
                matched = false;
                break;
              }
            }
            return matched;
          },
          targetDescr: "any target different from expected events",
          queue: this
        };
        this.mEventSeq.push(uniquenessChecker);
      }
    }

    var unexpectedSeq = aInvoker.unexpectedEventSeq;
    if (unexpectedSeq) {
      for (var idx = 0; idx < unexpectedSeq.length; idx++) {
        unexpectedSeq[idx].unexpected = true;
        unexpectedSeq[idx].async = false;
      }

      this.mEventSeq = this.mEventSeq.concat(unexpectedSeq);
    }

    this.mEventSeqIdx = -1;
    this.mEventSeqFinished = false;

    // Register event listeners
    if (this.mEventSeq) {
      aInvoker.wasCaught = new Array(this.mEventSeq.length);

      for (var idx = 0; idx < this.mEventSeq.length; idx++) {
        var eventType = this.getEventType(idx);

        if (gLogger.isEnabled()) {
          var msg = "registered";
          if (this.isEventUnexpected(idx))
            msg += " unexpected";

          msg += ": event type: " + this.getEventTypeAsString(idx) +
            ", target: " + this.getEventTargetDescr(idx, true);

          gLogger.logToConsole(msg);
          gLogger.logToDOM(msg, true);
        }

        if (typeof eventType == "string") {
          // DOM event
          var target = this.getEventTarget(idx);
          if (!target) {
            ok(false, "no target for DOM event!");
            return;
          }
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

  this.getEventTypeAsString = function eventQueue_getEventTypeAsString(aIdx)
  {
    var type = this.mEventSeq[aIdx].type;
    return (typeof type == "string") ? type : eventTypeToString(type);
  }

  this.getEventTarget = function eventQueue_getEventTarget(aIdx)
  {
    return this.mEventSeq[aIdx].target;
  }

  this.getEventTargetDescr =
    function eventQueue_getEventTargetDescr(aIdx, aDontForceTarget)
  {
    var descr = this.mEventSeq[aIdx].targetDescr;
    if (descr)
      return descr;

    if (aDontForceTarget)
      return "no target description";

    var target = ("target" in this.mEventSeq[aIdx]) ?
      this.mEventSeq[aIdx].target : null;
    return prettyName(target);
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

  this.isEventUnexpected = function eventQueue_isEventUnexpected(aIdx)
  {
    return this.mEventSeq[aIdx].unexpected;
  }
  this.isEventExpected = function eventQueue_isEventExpected(aIdx)
  {
    return !this.mEventSeq[aIdx].unexpected;
  }

  this.compareEventTypes = function eventQueue_compareEventTypes(aIdx, aEvent)
  {
    var eventType1 = this.getEventType(aIdx);
    var eventType2 = (aEvent instanceof nsIDOMEvent) ?
      aEvent.type : aEvent.eventType;

    return eventType1 == eventType2;
  }

  this.compareEvents = function eventQueue_compareEvents(aIdx, aEvent)
  {
    if (!this.compareEventTypes(aIdx, aEvent))
      return false;

    // If checker provides "match" function then allow the checker to decide
    // whether event is matched.
    if ("match" in this.mEventSeq[aIdx])
      return this.mEventSeq[aIdx].match(aEvent);

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

  this.isSameEvent = function eventQueue_isSameEvent(aIdx, aEvent)
  {
    // We don't have stored info about handled event other than its type and
    // target, thus we should filter text change and state change events since
    // they may occur on the same element because of complex changes.
    return this.compareEvents(aIdx, aEvent) &&
      !(aEvent instanceof nsIAccessibleTextChangeEvent) &&
      !(aEvent instanceof nsIAccessibleStateChangeEvent);
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
    if (!gLogger.isEnabled()) // debug stuff
      return;

    // Dump DOM event information. Skip a11y event since it is dumped by
    // gA11yEventObserver.
    if (aOrigEvent instanceof nsIDOMEvent) {
      var info = "Event type: " + aOrigEvent.type;
      info += ". Target: " + prettyName(aOrigEvent.originalTarget);
      gLogger.logToDOM(info);
    }

    if (!aMatch)
      return;

    var msg = "EQ: ";
    var emphText = "matched ";

    var currType = this.getEventTypeAsString(aExpectedEventIdx);
    var currTargetDescr = this.getEventTargetDescr(aExpectedEventIdx);
    var consoleMsg = "*****\nEQ matched: " + currType + "\n*****";
    gLogger.logToConsole(consoleMsg);

    msg += " event, type: " + currType + ", target: " + currTargetDescr;

    gLogger.logToDOM(msg, true, emphText);
  }

  this.mDefEventType = aEventType;

  this.mInvokers = new Array();
  this.mIndex = -1;

  this.mEventSeq = null;
  this.mEventSeqIdx = -1;
  this.mEventSeqFinished = false;
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
 * Invokers defined below take a checker object (or array of checker objects).
 * An invoker listens for default event type registered in event queue object
 * until its checker is provided.
 *
 * Note, checker object or array of checker objects is optional.
 */

/**
 * Click invoker.
 */
function synthClick(aNodeOrID, aCheckerOrEventSeq, aArgs)
{
  this.__proto__ = new synthAction(aNodeOrID, aCheckerOrEventSeq);

  this.invoke = function synthClick_invoke()
  {
    var targetNode = this.DOMNode;
    if (targetNode instanceof nsIDOMDocument) {
      targetNode =
        this.DOMNode.body ? this.DOMNode.body : this.DOMNode.documentElement;
    }

    // Scroll the node into view, otherwise synth click may fail.
    if (targetNode instanceof nsIDOMNSHTMLElement) {
      targetNode.scrollIntoView(true);
    } else if (targetNode instanceof nsIDOMXULElement) {
      var targetAcc = getAccessible(targetNode);
      targetAcc.scrollTo(SCROLL_TYPE_ANYWHERE);
    }

    var x = 1, y = 1;
    if (aArgs && ("where" in aArgs) && aArgs.where == "right") {
      if (targetNode instanceof nsIDOMNSHTMLElement)
        x = targetNode.offsetWidth - 1;
      else if (targetNode instanceof nsIDOMXULElement)
        x = targetNode.boxObject.width - 1;
    }
    synthesizeMouse(targetNode, x, y, aArgs ? aArgs : {});
  }

  this.finalCheck = function synthClick_finalCheck()
  {
    // Scroll top window back.
    window.top.scrollTo(0, 0);
  }

  this.getID = function synthClick_getID()
  {
    return prettyName(aNodeOrID) + " click";
  }
}

/**
 * Mouse move invoker.
 */
function synthMouseMove(aID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthAction(aID, aCheckerOrEventSeq);

  this.invoke = function synthMouseMove_invoke()
  {
    synthesizeMouse(this.DOMNode, 1, 1, { type: "mousemove" });
    synthesizeMouse(this.DOMNode, 2, 2, { type: "mousemove" });
  }

  this.getID = function synthMouseMove_getID()
  {
    return prettyName(aID) + " mouse move";
  }
}

/**
 * General key press invoker.
 */
function synthKey(aNodeOrID, aKey, aArgs, aCheckerOrEventSeq)
{
  this.__proto__ = new synthAction(aNodeOrID, aCheckerOrEventSeq);

  this.invoke = function synthKey_invoke()
  {
    synthesizeKey(this.mKey, this.mArgs, this.mWindow);
  }

  this.getID = function synthKey_getID()
  {
    var key = this.mKey;
    switch (this.mKey) {
      case "VK_TAB":
        key = "tab";
        break;
      case "VK_DOWN":
        key = "down";
        break;
      case "VK_UP":
        key = "up";
        break;
      case "VK_LEFT":
        key = "left";
        break;
      case "VK_RIGHT":
        key = "right";
        break;
      case "VK_HOME":
        key = "home";
        break;
      case "VK_ESCAPE":
        key = "escape";
        break;
      case "VK_RETURN":
        key = "enter";
        break;
    }
    if (aArgs) {
      if (aArgs.shiftKey)
        key += " shift";
      if (aArgs.ctrlKey)
        key += " ctrl";
      if (aArgs.altKey)
        key += " alt";
    }
    return prettyName(aNodeOrID) + " '" + key + " ' key";
  }

  this.mKey = aKey;
  this.mArgs = aArgs ? aArgs : {};
  this.mWindow = aArgs ? aArgs.window : null;
}

/**
 * Tab key invoker.
 */
function synthTab(aNodeOrID, aCheckerOrEventSeq, aWindow)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_TAB",
                                { shiftKey: false, window: aWindow },
                                aCheckerOrEventSeq);
}

/**
 * Shift tab key invoker.
 */
function synthShiftTab(aNodeOrID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_TAB", { shiftKey: true },
                                aCheckerOrEventSeq);
}

/**
 * Escape key invoker.
 */
function synthEscapeKey(aNodeOrID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_ESCAPE", null,
                                aCheckerOrEventSeq);
}

/**
 * Down arrow key invoker.
 */
function synthDownKey(aNodeOrID, aCheckerOrEventSeq, aArgs)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_DOWN", aArgs,
                                aCheckerOrEventSeq);
}

/**
 * Up arrow key invoker.
 */
function synthUpKey(aNodeOrID, aCheckerOrEventSeq, aArgs)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_UP", aArgs,
                                aCheckerOrEventSeq);
}

/**
 * Right arrow key invoker.
 */
function synthRightKey(aNodeOrID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_RIGHT", null, aCheckerOrEventSeq);
}

/**
 * Home key invoker.
 */
function synthHomeKey(aNodeOrID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthKey(aNodeOrID, "VK_HOME", null, aCheckerOrEventSeq);
}

/**
 * Enter key invoker
 */
function synthEnterKey(aID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthKey(aID, "VK_RETURN", null, aCheckerOrEventSeq);
}

/**
 * Synth alt + down arrow to open combobox.
 */
function synthOpenComboboxKey(aID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthDownKey(aID, aCheckerOrEventSeq, { altKey: true });

  this.getID = function synthOpenComboboxKey_getID()
  {
    return "open combobox (atl + down arrow) " + prettyName(aID);
  }
}

/**
 * Focus invoker.
 */
function synthFocus(aNodeOrID, aCheckerOrEventSeq)
{
  var checkerOfEventSeq =
    aCheckerOrEventSeq ? aCheckerOrEventSeq : new focusChecker(aNodeOrID);
  this.__proto__ = new synthAction(aNodeOrID, checkerOfEventSeq);

  this.invoke = function synthFocus_invoke()
  {
    if (this.DOMNode instanceof Components.interfaces.nsIDOMNSEditableElement &&
        this.DOMNode.editor ||
        this.DOMNode instanceof Components.interfaces.nsIDOMXULTextBoxElement) {
      this.DOMNode.selectionStart = this.DOMNode.selectionEnd = this.DOMNode.value.length;
    }
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
function synthFocusOnFrame(aNodeOrID, aCheckerOrEventSeq)
{
  var frameDoc = getNode(aNodeOrID).contentDocument;
  var checkerOrEventSeq =
    aCheckerOrEventSeq ? aCheckerOrEventSeq : new focusChecker(frameDoc);
  this.__proto__ = new synthAction(frameDoc, checkerOrEventSeq);

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
 * Change the current item when the widget doesn't have a focus.
 */
function changeCurrentItem(aID, aItemID)
{
  this.eventSeq = [ new nofocusChecker() ];

  this.invoke = function changeCurrentItem_invoke()
  {
    var controlNode = getNode(aID);
    var itemNode = getNode(aItemID);

    // HTML
    if (controlNode.localName == "input") {
      if (controlNode.checked)
        this.reportError();

      controlNode.checked = true;
      return;
    }

    if (controlNode.localName == "select") {
      if (controlNode.selectedIndex == itemNode.index)
        this.reportError();

      controlNode.selectedIndex = itemNode.index;
      return;
    }

    // XUL
    if (controlNode.localName == "tree") {
      if (controlNode.currentIndex == aItemID)
        this.reportError();

      controlNode.currentIndex = aItemID;
      return;
    }

    if (controlNode.localName == "menulist") {
      if (controlNode.selectedItem == itemNode)
        this.reportError();

      controlNode.selectedItem = itemNode;
      return;
    }

    if (controlNode.currentItem == itemNode)
      ok(false, "Error in test: proposed current item is already current" + prettyName(aID));

    controlNode.currentItem = itemNode;
  }

  this.getID = function changeCurrentItem_getID()
  {
    return "current item change for " + prettyName(aID);
  }

  this.reportError = function changeCurrentItem_reportError()
  {
    ok(false,
       "Error in test: proposed current item '" + aItemID + "' is already current");
  }
}

/**
 * Toggle top menu invoker.
 */
function toggleTopMenu(aID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthKey(aID, "VK_ALT", null,
                                aCheckerOrEventSeq);

  this.getID = function toggleTopMenu_getID()
  {
    return "toggle top menu on " + prettyName(aID);
  }
}

/**
 * Context menu invoker.
 */
function synthContextMenu(aID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthClick(aID, aCheckerOrEventSeq,
                                  { button: 0, type: "contextmenu" });

  this.getID = function synthContextMenu_getID()
  {
    return "context menu on " + prettyName(aID);
  }
}

/**
 * Open combobox, autocomplete and etc popup, check expandable states.
 */
function openCombobox(aComboboxID)
{
  this.eventSeq = [
    new stateChangeChecker(STATE_EXPANDED, false, true, aComboboxID)
  ];

  this.invoke = function openCombobox_invoke()
  {
    getNode(aComboboxID).focus();
    synthesizeKey("VK_DOWN", { altKey: true });
  }

  this.getID = function openCombobox_getID()
  {
    return "open combobox " + prettyName(aComboboxID);
  }
}

/**
 * Close combobox, autocomplete and etc popup, check expandable states.
 */
function closeCombobox(aComboboxID)
{
  this.eventSeq = [
    new stateChangeChecker(STATE_EXPANDED, false, false, aComboboxID)
  ];

  this.invoke = function closeCombobox_invoke()
  {
    synthesizeKey("VK_ESCAPE", { });
  }

  this.getID = function closeCombobox_getID()
  {
    return "close combobox " + prettyName(aComboboxID);
  }
}


/**
 * Select all invoker.
 */
function synthSelectAll(aNodeOrID, aCheckerOrEventSeq)
{
  this.__proto__ = new synthAction(aNodeOrID, aCheckerOrEventSeq);

  this.invoke = function synthSelectAll_invoke()
  {
    if (this.DOMNode instanceof Components.interfaces.nsIDOMHTMLInputElement ||
        this.DOMNode instanceof Components.interfaces.nsIDOMXULTextBoxElement) {
      this.DOMNode.select();

    } else {
      window.getSelection().selectAllChildren(this.DOMNode);
    }
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
function invokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg, aIsAsync)
{
  this.type = aEventType;
  this.async = aIsAsync;

  this.__defineGetter__("target", invokerChecker_targetGetter);
  this.__defineSetter__("target", invokerChecker_targetSetter);

  // implementation details
  function invokerChecker_targetGetter()
  {
    if (typeof this.mTarget == "function")
      return this.mTarget.call(null, this.mTargetFuncArg);
    if (typeof this.mTarget == "string")
      return getNode(this.mTarget);

    return this.mTarget;
  }

  function invokerChecker_targetSetter(aValue)
  {
    this.mTarget = aValue;
    return this.mTarget;
  }

  this.__defineGetter__("targetDescr", invokerChecker_targetDescrGetter);

  function invokerChecker_targetDescrGetter()
  {
    if (typeof this.mTarget == "function")
      return this.mTarget.name + ", arg: " + this.mTargetFuncArg;

    return prettyName(this.mTarget);
  }

  this.mTarget = aTargetOrFunc;
  this.mTargetFuncArg = aTargetFuncArg;
}

/**
 * Common invoker checker for async events.
 */
function asyncInvokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg)
{
  this.__proto__ = new invokerChecker(aEventType, aTargetOrFunc,
                                      aTargetFuncArg, true);
}

function focusChecker(aTargetOrFunc, aTargetFuncArg)
{
  this.__proto__ = new invokerChecker(EVENT_FOCUS, aTargetOrFunc,
                                      aTargetFuncArg, false);

  this.unique = true; // focus event must be unique for invoker action

  this.check = function focusChecker_check(aEvent)
  {
    testStates(aEvent.accessible, STATE_FOCUSED);
  }
}

function nofocusChecker(aID)
{
  this.__proto__ = new focusChecker(aID);
  this.unexpected = true;
}

/**
 * Text inserted/removed events checker.
 */
function textChangeChecker(aID, aStart, aEnd, aTextOrFunc, aIsInserted)
{
  this.target = getNode(aID);
  this.type = aIsInserted ? EVENT_TEXT_INSERTED : EVENT_TEXT_REMOVED;

  this.check = function textChangeChecker_check(aEvent)
  {
    aEvent.QueryInterface(nsIAccessibleTextChangeEvent);

    var modifiedText = (typeof aTextOrFunc == "function") ?
      aTextOrFunc() : aTextOrFunc;
    var modifiedTextLen = (aEnd == -1) ? modifiedText.length : aEnd - aStart;

    is(aEvent.start, aStart, "Wrong start offset for " + prettyName(aID));
    is(aEvent.length, modifiedTextLen, "Wrong length for " + prettyName(aID));
    var changeInfo = (aIsInserted ? "inserted" : "removed");
    is(aEvent.isInserted(), aIsInserted,
       "Text was " + changeInfo + " for " + prettyName(aID));
    is(aEvent.modifiedText, modifiedText,
       "Wrong " + changeInfo + " text for " + prettyName(aID));
  }
}

/**
 * Caret move events checker.
 */
function caretMoveChecker(aCaretOffset, aTargetOrFunc, aTargetFuncArg)
{
  this.__proto__ = new invokerChecker(EVENT_TEXT_CARET_MOVED,
                                      aTargetOrFunc, aTargetFuncArg);

  this.check = function caretMoveChecker_check(aEvent)
  {
    is(aEvent.QueryInterface(nsIAccessibleCaretMoveEvent).caretOffset,
       aCaretOffset,
       "Wrong caret offset for " + prettyName(aEvent.accessible));
  }
}

/**
 * State change checker.
 */
function stateChangeChecker(aState, aIsExtraState, aIsEnabled,
                            aTargetOrFunc, aTargetFuncArg)
{
  this.__proto__ = new invokerChecker(EVENT_STATE_CHANGE, aTargetOrFunc,
                                      aTargetFuncArg);

  this.check = function stateChangeChecker_check(aEvent)
  {
    var event = null;
    try {
      var event = aEvent.QueryInterface(nsIAccessibleStateChangeEvent);
    } catch (e) {
      ok(false, "State change event was expected");
    }

    if (!event)
      return;

    is(event.state, aState, "Wrong state of the statechange event.");
    is(event.isExtraState(), aIsExtraState,
       "Wrong extra state bit of the statechange event.");
    is(event.isEnabled(), aIsEnabled,
      "Wrong state of statechange event state");

    var state = aIsEnabled ? (aIsExtraState ? 0 : aState) : 0;
    var extraState = aIsEnabled ? (aIsExtraState ? aState : 0) : 0;
    var unxpdState = aIsEnabled ? 0 : (aIsExtraState ? 0 : aState);
    var unxpdExtraState = aIsEnabled ? 0 : (aIsExtraState ? aState : 0);
    testStates(event.accessible, state, extraState, unxpdState, unxpdExtraState);
  }
}

/**
 * Expanded state change checker.
 */
function expandedStateChecker(aIsEnabled, aTargetOrFunc, aTargetFuncArg)
{
  this.__proto__ = new invokerChecker(EVENT_STATE_CHANGE, aTargetOrFunc,
                                      aTargetFuncArg);

  this.check = function expandedStateChecker_check(aEvent)
  {
    var event = null;
    try {
      var event = aEvent.QueryInterface(nsIAccessibleStateChangeEvent);
    } catch (e) {
      ok(false, "State change event was expected");
    }

    if (!event)
      return;

    is(event.state, STATE_EXPANDED, "Wrong state of the statechange event.");
    is(event.isExtraState(), false,
       "Wrong extra state bit of the statechange event.");
    is(event.isEnabled(), aIsEnabled,
      "Wrong state of statechange event state");

    testStates(event.accessible,
               (aIsEnabled ? STATE_EXPANDED : STATE_COLLAPSED));
  }
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
    if (gLogger.isEnabled()) { // debug stuff
      eventFromDumpArea = true;

      var target = event.DOMNode;
      var dumpElm = gA11yEventDumpID ?
        document.getElementById(gA11yEventDumpID) : null;

      if (dumpElm) {
        var parent = target;
        while (parent && parent != dumpElm)
          parent = parent.parentNode;
      }

      if (!dumpElm || parent != dumpElm) {
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
        gLogger.log(info);
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

  var listenersArray = gA11yEventListeners[aEventType];
  var index = listenersArray.indexOf(aEventHandler);
  if (index == -1)
    listenersArray.push(aEventHandler);
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
 * Used to dump debug information.
 */
var gLogger =
{
  /**
   * Return true if dump is enabled.
   */
  isEnabled: function debugOutput_isEnabled()
  {
    return gA11yEventDumpID || gA11yEventDumpToConsole ||
      gA11yEventDumpToAppConsole;
  },

  /**
   * Dump information into DOM and console if applicable.
   */
  log: function logger_log(aMsg)
  {
    this.logToConsole(aMsg);
    this.logToAppConsole(aMsg);
    this.logToDOM(aMsg);
  },

  /**
   * Log message to DOM.
   *
   * @param aMsg          [in] the primary message
   * @param aHasIndent    [in, optional] if specified the message has an indent
   * @param aPreEmphText  [in, optional] the text is colored and appended prior
   *                        primary message
   */
  logToDOM: function logger_logToDOM(aMsg, aHasIndent, aPreEmphText)
  {
    if (gA11yEventDumpID == "")
      return;

    var dumpElm = document.getElementById(gA11yEventDumpID);
    if (!dumpElm) {
      ok(false,
         "No dump element '" + gA11yEventDumpID + "' within the document!");
      return;
    }

    var containerTagName = document instanceof nsIDOMHTMLDocument ?
      "div" : "description";

    var container = document.createElement(containerTagName);
    if (aHasIndent)
      container.setAttribute("style", "padding-left: 10px;");

    if (aPreEmphText) {
      var inlineTagName = document instanceof nsIDOMHTMLDocument ?
        "span" : "description";
      var emphElm = document.createElement(inlineTagName);
      emphElm.setAttribute("style", "color: blue;");
      emphElm.textContent = aPreEmphText;

      container.appendChild(emphElm);
    }

    var textNode = document.createTextNode(aMsg);
    container.appendChild(textNode);

    dumpElm.appendChild(container);
  },

  /**
   * Log message to console.
   */
  logToConsole: function logger_logToConsole(aMsg)
  {
    if (gA11yEventDumpToConsole)
      dump("\n" + aMsg + "\n");
  },

  /**
   * Log message to error console.
   */
  logToAppConsole: function logger_logToAppConsole(aMsg)
  {
    if (gA11yEventDumpToAppConsole)
      consoleService.logStringMessage("events: " + aMsg);
  },

  consoleService: Components.classes["@mozilla.org/consoleservice;1"].
    getService(Components.interfaces.nsIConsoleService)
};


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
function synthAction(aNodeOrID, aCheckerOrEventSeq)
{
  this.DOMNode = getNode(aNodeOrID);

  if (aCheckerOrEventSeq) {
    if (aCheckerOrEventSeq instanceof Array) {
      this.eventSeq = aCheckerOrEventSeq;
    } else {
      this.eventSeq = [ aCheckerOrEventSeq ];
    }
  }

  this.getID = function synthAction_getID()
    { return prettyName(aNodeOrID) + " action"; }
}
