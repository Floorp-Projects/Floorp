// XXX Bug 1425371 - enable no-redeclare and fix the issues with the tests.
/* eslint-disable no-redeclare */

// //////////////////////////////////////////////////////////////////////////////
// Constants

const EVENT_ALERT = nsIAccessibleEvent.EVENT_ALERT;
const EVENT_DESCRIPTION_CHANGE = nsIAccessibleEvent.EVENT_DESCRIPTION_CHANGE;
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
const EVENT_SELECTION = nsIAccessibleEvent.EVENT_SELECTION;
const EVENT_SELECTION_ADD = nsIAccessibleEvent.EVENT_SELECTION_ADD;
const EVENT_SELECTION_REMOVE = nsIAccessibleEvent.EVENT_SELECTION_REMOVE;
const EVENT_SELECTION_WITHIN = nsIAccessibleEvent.EVENT_SELECTION_WITHIN;
const EVENT_SHOW = nsIAccessibleEvent.EVENT_SHOW;
const EVENT_STATE_CHANGE = nsIAccessibleEvent.EVENT_STATE_CHANGE;
const EVENT_TEXT_ATTRIBUTE_CHANGED = nsIAccessibleEvent.EVENT_TEXT_ATTRIBUTE_CHANGED;
const EVENT_TEXT_CARET_MOVED = nsIAccessibleEvent.EVENT_TEXT_CARET_MOVED;
const EVENT_TEXT_INSERTED = nsIAccessibleEvent.EVENT_TEXT_INSERTED;
const EVENT_TEXT_REMOVED = nsIAccessibleEvent.EVENT_TEXT_REMOVED;
const EVENT_TEXT_SELECTION_CHANGED = nsIAccessibleEvent.EVENT_TEXT_SELECTION_CHANGED;
const EVENT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_VALUE_CHANGE;
const EVENT_TEXT_VALUE_CHANGE = nsIAccessibleEvent.EVENT_TEXT_VALUE_CHANGE;
const EVENT_VIRTUALCURSOR_CHANGED = nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED;

const kNotFromUserInput = 0;
const kFromUserInput = 1;

// //////////////////////////////////////////////////////////////////////////////
// General

Components.utils.import("resource://gre/modules/Services.jsm");

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
 * Semicolon separated set of logging features.
 */
var gA11yEventDumpFeature = "";

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
function waitForEvent(aEventType, aTargetOrFunc, aFunc, aContext, aArg1, aArg2) {
  var handler = {
    handleEvent: function handleEvent(aEvent) {

      var target = aTargetOrFunc;
      if (typeof aTargetOrFunc == "function")
        target = aTargetOrFunc.call();

      if (target) {
        if (target instanceof nsIAccessible &&
            target != aEvent.accessible)
          return;

        if (target instanceof nsIDOMNode &&
            target != aEvent.DOMNode)
          return;
      }

      unregisterA11yEventListener(aEventType, this);

      window.setTimeout(
        function() {
          aFunc.call(aContext, aArg1, aArg2);
        },
        0
      );
    }
  };

  registerA11yEventListener(aEventType, handler);
}

/**
 * Generate mouse move over image map what creates image map accessible (async).
 * See waitForImageMap() function.
 */
function waveOverImageMap(aImageMapID) {
  var imageMapNode = getNode(aImageMapID);
  synthesizeMouse(imageMapNode, 10, 10, { type: "mousemove" },
                  imageMapNode.ownerGlobal);
}

/**
 * Call the given function when the tree of the given image map is built.
 */
function waitForImageMap(aImageMapID, aTestFunc) {
  waveOverImageMap(aImageMapID);

  var imageMapAcc = getAccessible(aImageMapID);
  if (imageMapAcc.firstChild)
    return aTestFunc();

  waitForEvent(EVENT_REORDER, imageMapAcc, aTestFunc);
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
function registerA11yEventListener(aEventType, aEventHandler) {
  listenA11yEvents(true);
  addA11yEventListener(aEventType, aEventHandler);
}

/**
 * Unregister accessibility event listener. Must be called for every registered
 * event listener (see registerA11yEventListener() function) when the listener
 * is not needed.
 */
function unregisterA11yEventListener(aEventType, aEventHandler) {
  removeA11yEventListener(aEventType, aEventHandler);
  listenA11yEvents(false);
}


// //////////////////////////////////////////////////////////////////////////////
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
 *     // [optional] Is called when event of any registered type is handled.
 *     debugCheck: function(aEvent){},
 *
 *     // [ignored if 'eventSeq' is defined] DOM node event is generated for
 *     // (used in the case when invoker expects single event).
 *     DOMNode getter: function() {},
 *
 *     // [optional] if true then event sequences are ignored (no failure if
 *     // sequences are empty). Use you need to invoke an action, do some check
 *     // after timeout and proceed a next invoker.
 *     noEventsOnAction getter: function() {},
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
 *     //   match : function(aEvent) {},
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
 *   // Used to add a possible scenario of expected/unexpected events on
 *   // invoker's action.
 *  defineScenario(aInvokerObj, aEventSeq, aUnexpectedEventSeq)
 *
 *
 * @param  aEventType  [in, optional] the default event type (isn't used if
 *                      invoker defines eventSeq property).
 */
function eventQueue(aEventType) {
  // public

  /**
   * Add invoker object into queue.
   */
  this.push = function eventQueue_push(aEventInvoker) {
    this.mInvokers.push(aEventInvoker);
  };

  /**
   * Start the queue processing.
   */
  this.invoke = function eventQueue_invoke() {
    listenA11yEvents(true);

    // XXX: Intermittent test_events_caretmove.html fails withouth timeout,
    // see bug 474952.
    this.processNextInvokerInTimeout(true);
  };

  /**
   * This function is called when all events in the queue were handled.
   * Override it if you need to be notified of this.
   */
  this.onFinish = function eventQueue_finish() {
  };

  // private

  /**
   * Process next invoker.
   */
  this.processNextInvoker = function eventQueue_processNextInvoker() {
    // Some scenario was matched, we wait on next invoker processing.
    if (this.mNextInvokerStatus == kInvokerCanceled) {
      this.setInvokerStatus(kInvokerNotScheduled,
                            "scenario was matched, wait for next invoker activation");
      return;
    }

    this.setInvokerStatus(kInvokerNotScheduled, "the next invoker is processed now");

    // Finish processing of the current invoker if any.
    var testFailed = false;

    var invoker = this.getInvoker();
    if (invoker) {
      if ("finalCheck" in invoker)
        invoker.finalCheck();

      if (this.mScenarios && this.mScenarios.length) {
        var matchIdx = -1;
        for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
          var eventSeq = this.mScenarios[scnIdx];
          if (!this.areExpectedEventsLeft(eventSeq)) {
            for (var idx = 0; idx < eventSeq.length; idx++) {
              var checker = eventSeq[idx];
              if (checker.unexpected && checker.wasCaught ||
                  !checker.unexpected && checker.wasCaught != 1) {
                break;
              }
            }

            // Ok, we have matched scenario. Report it was completed ok. In
            // case of empty scenario guess it was matched but if later we
            // find out that non empty scenario was matched then it will be
            // a final match.
            if (idx == eventSeq.length) {
              if (matchIdx != -1 && eventSeq.length > 0 &&
                  this.mScenarios[matchIdx].length > 0) {
                ok(false,
                   "We have a matched scenario at index " + matchIdx + " already.");
              }

              if (matchIdx == -1 || eventSeq.length > 0)
                matchIdx = scnIdx;

              // Report everything is ok.
              for (var idx = 0; idx < eventSeq.length; idx++) {
                var checker = eventSeq[idx];

                var typeStr = eventQueue.getEventTypeAsString(checker);
                var msg = "Test with ID = '" + this.getEventID(checker) +
                  "' succeed. ";

                if (checker.unexpected) {
                  ok(true, msg + `There's no unexpected '${typeStr}' event.`);
                } else if (checker.todo) {
                  todo(false, `Todo event '${typeStr}' was caught`);
                } else {
                  ok(true, `${msg} Event '${typeStr}' was handled.`);
                }
              }
            }
          }
        }

        // We don't have completely matched scenario. Report each failure/success
        // for every scenario.
        if (matchIdx == -1) {
          testFailed = true;
          for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
            var eventSeq = this.mScenarios[scnIdx];
            for (var idx = 0; idx < eventSeq.length; idx++) {
              var checker = eventSeq[idx];

              var typeStr = eventQueue.getEventTypeAsString(checker);
              var msg = "Scenario #" + scnIdx + " of test with ID = '" +
                this.getEventID(checker) + "' failed. ";

              if (checker.wasCaught > 1)
                ok(false, msg + "Dupe " + typeStr + " event.");

              if (checker.unexpected) {
                if (checker.wasCaught) {
                  ok(false, msg + "There's unexpected " + typeStr + " event.");
                }
              } else if (!checker.wasCaught) {
                var rf = checker.todo ? todo : ok;
                rf(false, `${msg} '${typeStr} event is missed.`);
              }
            }
          }
        }
      }
    }

    this.clearEventHandler();

    // Check if need to stop the test.
    if (testFailed || this.mIndex == this.mInvokers.length - 1) {
      listenA11yEvents(false);

      var res = this.onFinish();
      if (res != DO_NOT_FINISH_TEST)
        SimpleTest.executeSoon(SimpleTest.finish);

      return;
    }

    // Start processing of next invoker.
    invoker = this.getNextInvoker();

    // Set up event listeners. Process a next invoker if no events were added.
    if (!this.setEventHandler(invoker)) {
      this.processNextInvoker();
      return;
    }

    if (gLogger.isEnabled()) {
      gLogger.logToConsole("Event queue: \n  invoke: " + invoker.getID());
      gLogger.logToDOM("EQ: invoke: " + invoker.getID(), true);
    }

    var infoText = "Invoke the '" + invoker.getID() + "' test { ";
    var scnCount = this.mScenarios ? this.mScenarios.length : 0;
    for (var scnIdx = 0; scnIdx < scnCount; scnIdx++) {
      infoText += "scenario #" + scnIdx + ": ";
      var eventSeq = this.mScenarios[scnIdx];
      for (var idx = 0; idx < eventSeq.length; idx++) {
        infoText += eventSeq[idx].unexpected ? "un" : "" +
          "expected '" + eventQueue.getEventTypeAsString(eventSeq[idx]) +
          "' event; ";
      }
    }
    infoText += " }";
    info(infoText);

    if (invoker.invoke() == INVOKER_ACTION_FAILED) {
      // Invoker failed to prepare action, fail and finish tests.
      this.processNextInvoker();
      return;
    }

    if (this.hasUnexpectedEventsScenario())
      this.processNextInvokerInTimeout(true);
  };

  this.processNextInvokerInTimeout =
    function eventQueue_processNextInvokerInTimeout(aUncondProcess) {
    this.setInvokerStatus(kInvokerPending, "Process next invoker in timeout");

    // No need to wait extra timeout when a) we know we don't need to do that
    // and b) there's no any single unexpected event.
    if (!aUncondProcess && this.areAllEventsExpected()) {
      // We need delay to avoid events coalesce from different invokers.
      var queue = this;
      SimpleTest.executeSoon(function() { queue.processNextInvoker(); });
      return;
    }

    // Check in timeout invoker didn't fire registered events.
    window.setTimeout(function(aQueue) { aQueue.processNextInvoker(); }, 300,
                      this);
  };

  /**
   * Handle events for the current invoker.
   */
  this.handleEvent = function eventQueue_handleEvent(aEvent) {
    var invoker = this.getInvoker();
    if (!invoker) // skip events before test was started
      return;

    if (!this.mScenarios) {
      // Bad invoker object, error will be reported before processing of next
      // invoker in the queue.
      this.processNextInvoker();
      return;
    }

    if ("debugCheck" in invoker)
      invoker.debugCheck(aEvent);

    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      var eventSeq = this.mScenarios[scnIdx];
      for (var idx = 0; idx < eventSeq.length; idx++) {
        var checker = eventSeq[idx];

        // Search through handled expected events to report error if one of them
        // is handled for a second time.
        if (!checker.unexpected && (checker.wasCaught > 0) &&
            eventQueue.isSameEvent(checker, aEvent)) {
          checker.wasCaught++;
          continue;
        }

        // Search through unexpected events, any match results in error report
        // after this invoker processing (in case of matched scenario only).
        if (checker.unexpected && eventQueue.compareEvents(checker, aEvent)) {
          checker.wasCaught++;
          continue;
        }

        // Report an error if we hanlded not expected event of unique type
        // (i.e. event types are matched, targets differs).
        if (!checker.unexpected && checker.unique &&
            eventQueue.compareEventTypes(checker, aEvent)) {
          var isExpected = false;
          for (var jdx = 0; jdx < eventSeq.length; jdx++) {
            isExpected = eventQueue.compareEvents(eventSeq[jdx], aEvent);
            if (isExpected)
              break;
          }

          if (!isExpected) {
            ok(false,
               "Unique type " +
               eventQueue.getEventTypeAsString(checker) + " event was handled.");
          }
        }
      }
    }

    var hasMatchedCheckers = false;
    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      var eventSeq = this.mScenarios[scnIdx];

      // Check if handled event matches expected sync event.
      var nextChecker = this.getNextExpectedEvent(eventSeq);
      if (nextChecker) {
        if (eventQueue.compareEvents(nextChecker, aEvent)) {
          this.processMatchedChecker(aEvent, nextChecker, scnIdx, eventSeq.idx);
          hasMatchedCheckers = true;
          continue;
        }
      }

      // Check if handled event matches any expected async events.
      var haveUnmatchedAsync = false;
      for (idx = 0; idx < eventSeq.length; idx++) {
        if (eventSeq[idx] instanceof orderChecker && haveUnmatchedAsync) {
            break;
        }

        if (!eventSeq[idx].wasCaught) {
          haveUnmatchedAsync = true;
        }

        if (!eventSeq[idx].unexpected && eventSeq[idx].async) {
          if (eventQueue.compareEvents(eventSeq[idx], aEvent)) {
            this.processMatchedChecker(aEvent, eventSeq[idx], scnIdx, idx);
            hasMatchedCheckers = true;
            break;
          }
        }
      }
    }

    if (hasMatchedCheckers) {
      var invoker = this.getInvoker();
      if ("check" in invoker)
        invoker.check(aEvent);
    }

    for (idx = 0; idx < eventSeq.length; idx++) {
      if (!eventSeq[idx].wasCaught) {
        if (eventSeq[idx] instanceof orderChecker) {
          eventSeq[idx].wasCaught++;
        } else {
          break;
        }
      }
    }

    // If we don't have more events to wait then schedule next invoker.
    if (this.hasMatchedScenario()) {
      if (this.mNextInvokerStatus == kInvokerNotScheduled) {
        this.processNextInvokerInTimeout();

      } else if (this.mNextInvokerStatus == kInvokerCanceled) {
        this.setInvokerStatus(kInvokerPending,
                              "Full match. Void the cancelation of next invoker processing");
      }
      return;
    }

    // If we have scheduled a next invoker then cancel in case of match.
    if ((this.mNextInvokerStatus == kInvokerPending) && hasMatchedCheckers) {
      this.setInvokerStatus(kInvokerCanceled,
                            "Cancel the scheduled invoker in case of match");
    }
  };

  // Helpers
  this.processMatchedChecker =
    function eventQueue_function(aEvent, aMatchedChecker, aScenarioIdx, aEventIdx) {
    aMatchedChecker.wasCaught++;

    if ("check" in aMatchedChecker)
      aMatchedChecker.check(aEvent);

    eventQueue.logEvent(aEvent, aMatchedChecker, aScenarioIdx, aEventIdx,
                        this.areExpectedEventsLeft(),
                        this.mNextInvokerStatus);
  };

  this.getNextExpectedEvent =
    function eventQueue_getNextExpectedEvent(aEventSeq) {
    if (!("idx" in aEventSeq))
      aEventSeq.idx = 0;

    while (aEventSeq.idx < aEventSeq.length &&
           (aEventSeq[aEventSeq.idx].unexpected ||
            aEventSeq[aEventSeq.idx].todo ||
            aEventSeq[aEventSeq.idx].async ||
            aEventSeq[aEventSeq.idx] instanceof orderChecker ||
            aEventSeq[aEventSeq.idx].wasCaught > 0)) {
      aEventSeq.idx++;
    }

    return aEventSeq.idx != aEventSeq.length ? aEventSeq[aEventSeq.idx] : null;
  };

  this.areExpectedEventsLeft =
    function eventQueue_areExpectedEventsLeft(aScenario) {
      function scenarioHasUnhandledExpectedEvent(aEventSeq) {
        // Check if we have unhandled async (can be anywhere in the sequance) or
        // sync expcected events yet.
        for (var idx = 0; idx < aEventSeq.length; idx++) {
          if (!aEventSeq[idx].unexpected && !aEventSeq[idx].todo &&
              !aEventSeq[idx].wasCaught && !(aEventSeq[idx] instanceof orderChecker))
            return true;
        }

        return false;
      }

      if (aScenario)
        return scenarioHasUnhandledExpectedEvent(aScenario);

      for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
        var eventSeq = this.mScenarios[scnIdx];
        if (scenarioHasUnhandledExpectedEvent(eventSeq))
          return true;
      }
      return false;
    };

  this.areAllEventsExpected =
    function eventQueue_areAllEventsExpected() {
    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      var eventSeq = this.mScenarios[scnIdx];
      for (var idx = 0; idx < eventSeq.length; idx++) {
        if (eventSeq[idx].unexpected || eventSeq[idx].todo)
          return false;
      }
    }

    return true;
  };

  this.isUnexpectedEventScenario =
    function eventQueue_isUnexpectedEventsScenario(aScenario) {
    for (var idx = 0; idx < aScenario.length; idx++) {
      if (!aScenario[idx].unexpected && !aScenario[idx].todo)
        break;
    }

    return idx == aScenario.length;
  };

  this.hasUnexpectedEventsScenario =
    function eventQueue_hasUnexpectedEventsScenario() {
    if (this.getInvoker().noEventsOnAction)
      return true;

    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      if (this.isUnexpectedEventScenario(this.mScenarios[scnIdx]))
        return true;
    }

    return false;
  };

  this.hasMatchedScenario =
    function eventQueue_hasMatchedScenario() {
    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      var scn = this.mScenarios[scnIdx];
      if (!this.isUnexpectedEventScenario(scn) && !this.areExpectedEventsLeft(scn))
        return true;
    }
    return false;
  };

  this.getInvoker = function eventQueue_getInvoker() {
    return this.mInvokers[this.mIndex];
  };

  this.getNextInvoker = function eventQueue_getNextInvoker() {
    return this.mInvokers[++this.mIndex];
  };

  this.setEventHandler = function eventQueue_setEventHandler(aInvoker) {
    if (!("scenarios" in aInvoker) || aInvoker.scenarios.length == 0) {
      var eventSeq = aInvoker.eventSeq;
      var unexpectedEventSeq = aInvoker.unexpectedEventSeq;
      if (!eventSeq && !unexpectedEventSeq && this.mDefEventType)
        eventSeq = [ new invokerChecker(this.mDefEventType, aInvoker.DOMNode) ];

      if (eventSeq || unexpectedEventSeq)
        defineScenario(aInvoker, eventSeq, unexpectedEventSeq);
    }

    if (aInvoker.noEventsOnAction)
      return true;

    this.mScenarios = aInvoker.scenarios;
    if (!this.mScenarios || !this.mScenarios.length) {
      ok(false, "Broken invoker '" + aInvoker.getID() + "'");
      return false;
    }

    // Register event listeners.
    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      var eventSeq = this.mScenarios[scnIdx];

      if (gLogger.isEnabled()) {
        var msg = "scenario #" + scnIdx +
          ", registered events number: " + eventSeq.length;
        gLogger.logToConsole(msg);
        gLogger.logToDOM(msg, true);
      }

      // Do not warn about empty event sequances when more than one scenario
      // was registered.
      if (this.mScenarios.length == 1 && eventSeq.length == 0) {
        ok(false,
           "Broken scenario #" + scnIdx + " of invoker '" + aInvoker.getID() +
           "'. No registered events");
        return false;
      }

      for (var idx = 0; idx < eventSeq.length; idx++)
        eventSeq[idx].wasCaught = 0;

      for (var idx = 0; idx < eventSeq.length; idx++) {
        if (gLogger.isEnabled()) {
          var msg = "registered";
          if (eventSeq[idx].unexpected)
            msg += " unexpected";
          if (eventSeq[idx].async)
            msg += " async";

          msg += ": event type: " +
            eventQueue.getEventTypeAsString(eventSeq[idx]) +
            ", target: " + eventQueue.getEventTargetDescr(eventSeq[idx], true);

          gLogger.logToConsole(msg);
          gLogger.logToDOM(msg, true);
        }

        var eventType = eventSeq[idx].type;
        if (typeof eventType == "string") {
          // DOM event
          var target = eventQueue.getEventTarget(eventSeq[idx]);
          if (!target) {
            ok(false, "no target for DOM event!");
            return false;
          }
          var phase = eventQueue.getEventPhase(eventSeq[idx]);
          target.addEventListener(eventType, this, phase);

        } else {
          // A11y event
          addA11yEventListener(eventType, this);
        }
      }
    }

    return true;
  };

  this.clearEventHandler = function eventQueue_clearEventHandler() {
    if (!this.mScenarios)
      return;

    for (var scnIdx = 0; scnIdx < this.mScenarios.length; scnIdx++) {
      var eventSeq = this.mScenarios[scnIdx];
      for (var idx = 0; idx < eventSeq.length; idx++) {
        var eventType = eventSeq[idx].type;
        if (typeof eventType == "string") {
          // DOM event
          var target = eventQueue.getEventTarget(eventSeq[idx]);
          var phase = eventQueue.getEventPhase(eventSeq[idx]);
          target.removeEventListener(eventType, this, phase);

        } else {
          // A11y event
          removeA11yEventListener(eventType, this);
        }
      }
    }
    this.mScenarios = null;
  };

  this.getEventID = function eventQueue_getEventID(aChecker) {
    if ("getID" in aChecker)
      return aChecker.getID();

    var invoker = this.getInvoker();
    return invoker.getID();
  };

  this.setInvokerStatus = function eventQueue_setInvokerStatus(aStatus, aLogMsg) {
    this.mNextInvokerStatus = aStatus;

    // Uncomment it to debug invoker processing logic.
    // gLogger.log(eventQueue.invokerStatusToMsg(aStatus, aLogMsg));
  };

  this.mDefEventType = aEventType;

  this.mInvokers = [];
  this.mIndex = -1;
  this.mScenarios = null;

  this.mNextInvokerStatus = kInvokerNotScheduled;
}

// //////////////////////////////////////////////////////////////////////////////
// eventQueue static members and constants

const kInvokerNotScheduled = 0;
const kInvokerPending = 1;
const kInvokerCanceled = 2;

eventQueue.getEventTypeAsString =
  function eventQueue_getEventTypeAsString(aEventOrChecker) {
  if (aEventOrChecker instanceof nsIDOMEvent)
    return aEventOrChecker.type;

  if (aEventOrChecker instanceof nsIAccessibleEvent)
    return eventTypeToString(aEventOrChecker.eventType);

  return (typeof aEventOrChecker.type == "string") ?
    aEventOrChecker.type : eventTypeToString(aEventOrChecker.type);
};

eventQueue.getEventTargetDescr =
  function eventQueue_getEventTargetDescr(aEventOrChecker, aDontForceTarget) {
  if (aEventOrChecker instanceof nsIDOMEvent)
    return prettyName(aEventOrChecker.originalTarget);

  if (aEventOrChecker instanceof nsIDOMEvent)
    return prettyName(aEventOrChecker.accessible);

  var descr = aEventOrChecker.targetDescr;
  if (descr)
    return descr;

  if (aDontForceTarget)
    return "no target description";

  var target = ("target" in aEventOrChecker) ? aEventOrChecker.target : null;
  return prettyName(target);
};

eventQueue.getEventPhase = function eventQueue_getEventPhase(aChecker) {
  return ("phase" in aChecker) ? aChecker.phase : true;
};

eventQueue.getEventTarget = function eventQueue_getEventTarget(aChecker) {
  if ("eventTarget" in aChecker) {
    switch (aChecker.eventTarget) {
      case "element":
        return aChecker.target;
      case "document":
      default:
        return aChecker.target.ownerDocument;
    }
  }
  return aChecker.target.ownerDocument;
};

eventQueue.compareEventTypes =
  function eventQueue_compareEventTypes(aChecker, aEvent) {
  var eventType = (aEvent instanceof nsIDOMEvent) ?
    aEvent.type : aEvent.eventType;
  return aChecker.type == eventType;
};

eventQueue.compareEvents = function eventQueue_compareEvents(aChecker, aEvent) {
  if (!eventQueue.compareEventTypes(aChecker, aEvent))
    return false;

  // If checker provides "match" function then allow the checker to decide
  // whether event is matched.
  if ("match" in aChecker)
    return aChecker.match(aEvent);

  var target1 = aChecker.target;
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
};

eventQueue.isSameEvent = function eventQueue_isSameEvent(aChecker, aEvent) {
  // We don't have stored info about handled event other than its type and
  // target, thus we should filter text change and state change events since
  // they may occur on the same element because of complex changes.
  return this.compareEvents(aChecker, aEvent) &&
    !(aEvent instanceof nsIAccessibleTextChangeEvent) &&
    !(aEvent instanceof nsIAccessibleStateChangeEvent);
};

eventQueue.invokerStatusToMsg =
  function eventQueue_invokerStatusToMsg(aInvokerStatus, aMsg) {
  var msg = "invoker status: ";
  switch (aInvokerStatus) {
    case kInvokerNotScheduled:
      msg += "not scheduled";
      break;
    case kInvokerPending:
      msg += "pending";
      break;
    case kInvokerCanceled:
      msg += "canceled";
      break;
  }

  if (aMsg)
    msg += " (" + aMsg + ")";

  return msg;
};

eventQueue.logEvent = function eventQueue_logEvent(aOrigEvent, aMatchedChecker,
                                                   aScenarioIdx, aEventIdx,
                                                   aAreExpectedEventsLeft,
                                                   aInvokerStatus) {
  // Dump DOM event information. Skip a11y event since it is dumped by
  // gA11yEventObserver.
  if (aOrigEvent instanceof nsIDOMEvent) {
    var info = "Event type: " + eventQueue.getEventTypeAsString(aOrigEvent);
    info += ". Target: " + eventQueue.getEventTargetDescr(aOrigEvent);
    gLogger.logToDOM(info);
  }

  var infoMsg = "unhandled expected events: " + aAreExpectedEventsLeft +
    ", " + eventQueue.invokerStatusToMsg(aInvokerStatus);

  var currType = eventQueue.getEventTypeAsString(aMatchedChecker);
  var currTargetDescr = eventQueue.getEventTargetDescr(aMatchedChecker);
  var consoleMsg = "*****\nScenario " + aScenarioIdx +
    ", event " + aEventIdx + " matched: " + currType + "\n" + infoMsg + "\n*****";
  gLogger.logToConsole(consoleMsg);

  var emphText = "matched ";
  var msg = "EQ event, type: " + currType + ", target: " + currTargetDescr +
    ", " + infoMsg;
  gLogger.logToDOM(msg, true, emphText);
};


// //////////////////////////////////////////////////////////////////////////////
// Action sequence

/**
 * Deal with action sequence. Used when you need to execute couple of actions
 * each after other one.
 */
function sequence() {
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
                                         aItemID) {
    var item = new sequenceItem(aProcessor, aEventType, aTarget, aItemID);
    this.items.push(item);
  };

  /**
   * Process next sequence item.
   */
  this.processNext = function sequence_processNext() {
    this.idx++;
    if (this.idx >= this.items.length) {
      ok(false, "End of sequence: nothing to process!");
      SimpleTest.finish();
      return;
    }

    this.items[this.idx].startProcess();
  };

  this.items = [];
  this.idx = -1;
}


// //////////////////////////////////////////////////////////////////////////////
// Event queue invokers

/**
 * Defines a scenario of expected/unexpected events. Each invoker can have
 * one or more scenarios of events. Only one scenario must be completed.
 */
function defineScenario(aInvoker, aEventSeq, aUnexpectedEventSeq) {
  if (!("scenarios" in aInvoker))
    aInvoker.scenarios = [];

  // Create unified event sequence concatenating expected and unexpected
  // events.
  if (!aEventSeq)
    aEventSeq = [];

  for (var idx = 0; idx < aEventSeq.length; idx++) {
    aEventSeq[idx].unexpected |= false;
    aEventSeq[idx].async |= false;
  }

  if (aUnexpectedEventSeq) {
    for (var idx = 0; idx < aUnexpectedEventSeq.length; idx++) {
      aUnexpectedEventSeq[idx].unexpected = true;
      aUnexpectedEventSeq[idx].async = false;
    }

    aEventSeq = aEventSeq.concat(aUnexpectedEventSeq);
  }

  aInvoker.scenarios.push(aEventSeq);
}


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
function synthClick(aNodeOrID, aCheckerOrEventSeq, aArgs) {
  this.__proto__ = new synthAction(aNodeOrID, aCheckerOrEventSeq);

  this.invoke = function synthClick_invoke() {
    var targetNode = this.DOMNode;
    if (targetNode instanceof nsIDOMDocument) {
      targetNode =
        this.DOMNode.body ? this.DOMNode.body : this.DOMNode.documentElement;
    }

    // Scroll the node into view, otherwise synth click may fail.
    if (targetNode instanceof nsIDOMHTMLElement) {
      targetNode.scrollIntoView(true);
    } else if (targetNode instanceof nsIDOMXULElement) {
      var targetAcc = getAccessible(targetNode);
      targetAcc.scrollTo(SCROLL_TYPE_ANYWHERE);
    }

    var x = 1, y = 1;
    if (aArgs && ("where" in aArgs) && aArgs.where == "right") {
      if (targetNode instanceof nsIDOMHTMLElement)
        x = targetNode.offsetWidth - 1;
      else if (targetNode instanceof nsIDOMXULElement)
        x = targetNode.boxObject.width - 1;
    }
    synthesizeMouse(targetNode, x, y, aArgs ? aArgs : {});
  };

  this.finalCheck = function synthClick_finalCheck() {
    // Scroll top window back.
    window.top.scrollTo(0, 0);
  };

  this.getID = function synthClick_getID() {
    return prettyName(aNodeOrID) + " click";
  };
}

/**
 * Mouse move invoker.
 */
function synthMouseMove(aID, aCheckerOrEventSeq) {
  this.__proto__ = new synthAction(aID, aCheckerOrEventSeq);

  this.invoke = function synthMouseMove_invoke() {
    synthesizeMouse(this.DOMNode, 1, 1, { type: "mousemove" });
    synthesizeMouse(this.DOMNode, 2, 2, { type: "mousemove" });
  };

  this.getID = function synthMouseMove_getID() {
    return prettyName(aID) + " mouse move";
  };
}

/**
 * General key press invoker.
 */
function synthKey(aNodeOrID, aKey, aArgs, aCheckerOrEventSeq) {
  this.__proto__ = new synthAction(aNodeOrID, aCheckerOrEventSeq);

  this.invoke = function synthKey_invoke() {
    synthesizeKey(this.mKey, this.mArgs, this.mWindow);
  };

  this.getID = function synthKey_getID() {
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
      case "VK_END":
        key = "end";
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
  };

  this.mKey = aKey;
  this.mArgs = aArgs ? aArgs : {};
  this.mWindow = aArgs ? aArgs.window : null;
}

/**
 * Tab key invoker.
 */
function synthTab(aNodeOrID, aCheckerOrEventSeq, aWindow) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_TAB",
                                { shiftKey: false, window: aWindow },
                                aCheckerOrEventSeq);
}

/**
 * Shift tab key invoker.
 */
function synthShiftTab(aNodeOrID, aCheckerOrEventSeq) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_TAB", { shiftKey: true },
                                aCheckerOrEventSeq);
}

/**
 * Escape key invoker.
 */
function synthEscapeKey(aNodeOrID, aCheckerOrEventSeq) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_ESCAPE", null,
                                aCheckerOrEventSeq);
}

/**
 * Down arrow key invoker.
 */
function synthDownKey(aNodeOrID, aCheckerOrEventSeq, aArgs) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_DOWN", aArgs,
                                aCheckerOrEventSeq);
}

/**
 * Up arrow key invoker.
 */
function synthUpKey(aNodeOrID, aCheckerOrEventSeq, aArgs) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_UP", aArgs,
                                aCheckerOrEventSeq);
}

/**
 * Left arrow key invoker.
 */
function synthLeftKey(aNodeOrID, aCheckerOrEventSeq, aArgs) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_LEFT", aArgs, aCheckerOrEventSeq);
}

/**
 * Right arrow key invoker.
 */
function synthRightKey(aNodeOrID, aCheckerOrEventSeq, aArgs) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_RIGHT", aArgs, aCheckerOrEventSeq);
}

/**
 * Home key invoker.
 */
function synthHomeKey(aNodeOrID, aCheckerOrEventSeq) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_HOME", null, aCheckerOrEventSeq);
}

/**
 * End key invoker.
 */
function synthEndKey(aNodeOrID, aCheckerOrEventSeq) {
  this.__proto__ = new synthKey(aNodeOrID, "VK_END", null, aCheckerOrEventSeq);
}

/**
 * Enter key invoker
 */
function synthEnterKey(aID, aCheckerOrEventSeq) {
  this.__proto__ = new synthKey(aID, "VK_RETURN", null, aCheckerOrEventSeq);
}

/**
 * Synth alt + down arrow to open combobox.
 */
function synthOpenComboboxKey(aID, aCheckerOrEventSeq) {
  this.__proto__ = new synthDownKey(aID, aCheckerOrEventSeq, { altKey: true });

  this.getID = function synthOpenComboboxKey_getID() {
    return "open combobox (atl + down arrow) " + prettyName(aID);
  };
}

/**
 * Focus invoker.
 */
function synthFocus(aNodeOrID, aCheckerOrEventSeq) {
  var checkerOfEventSeq =
    aCheckerOrEventSeq ? aCheckerOrEventSeq : new focusChecker(aNodeOrID);
  this.__proto__ = new synthAction(aNodeOrID, checkerOfEventSeq);

  this.invoke = function synthFocus_invoke() {
    if (this.DOMNode instanceof Components.interfaces.nsIDOMNSEditableElement &&
        this.DOMNode.editor ||
        this.DOMNode instanceof Components.interfaces.nsIDOMXULTextBoxElement) {
      this.DOMNode.selectionStart = this.DOMNode.selectionEnd = this.DOMNode.value.length;
    }
    this.DOMNode.focus();
  };

  this.getID = function synthFocus_getID() {
    return prettyName(aNodeOrID) + " focus";
  };
}

/**
 * Focus invoker. Focus the HTML body of content document of iframe.
 */
function synthFocusOnFrame(aNodeOrID, aCheckerOrEventSeq) {
  var frameDoc = getNode(aNodeOrID).contentDocument;
  var checkerOrEventSeq =
    aCheckerOrEventSeq ? aCheckerOrEventSeq : new focusChecker(frameDoc);
  this.__proto__ = new synthAction(frameDoc, checkerOrEventSeq);

  this.invoke = function synthFocus_invoke() {
    this.DOMNode.body.focus();
  };

  this.getID = function synthFocus_getID() {
    return prettyName(aNodeOrID) + " frame document focus";
  };
}

/**
 * Change the current item when the widget doesn't have a focus.
 */
function changeCurrentItem(aID, aItemID) {
  this.eventSeq = [ new nofocusChecker() ];

  this.invoke = function changeCurrentItem_invoke() {
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
  };

  this.getID = function changeCurrentItem_getID() {
    return "current item change for " + prettyName(aID);
  };

  this.reportError = function changeCurrentItem_reportError() {
    ok(false,
       "Error in test: proposed current item '" + aItemID + "' is already current");
  };
}

/**
 * Toggle top menu invoker.
 */
function toggleTopMenu(aID, aCheckerOrEventSeq) {
  this.__proto__ = new synthKey(aID, "VK_ALT", null,
                                aCheckerOrEventSeq);

  this.getID = function toggleTopMenu_getID() {
    return "toggle top menu on " + prettyName(aID);
  };
}

/**
 * Context menu invoker.
 */
function synthContextMenu(aID, aCheckerOrEventSeq) {
  this.__proto__ = new synthClick(aID, aCheckerOrEventSeq,
                                  { button: 0, type: "contextmenu" });

  this.getID = function synthContextMenu_getID() {
    return "context menu on " + prettyName(aID);
  };
}

/**
 * Open combobox, autocomplete and etc popup, check expandable states.
 */
function openCombobox(aComboboxID) {
  this.eventSeq = [
    new stateChangeChecker(STATE_EXPANDED, false, true, aComboboxID)
  ];

  this.invoke = function openCombobox_invoke() {
    getNode(aComboboxID).focus();
    synthesizeKey("VK_DOWN", { altKey: true });
  };

  this.getID = function openCombobox_getID() {
    return "open combobox " + prettyName(aComboboxID);
  };
}

/**
 * Close combobox, autocomplete and etc popup, check expandable states.
 */
function closeCombobox(aComboboxID) {
  this.eventSeq = [
    new stateChangeChecker(STATE_EXPANDED, false, false, aComboboxID)
  ];

  this.invoke = function closeCombobox_invoke() {
    synthesizeKey("VK_ESCAPE", { });
  };

  this.getID = function closeCombobox_getID() {
    return "close combobox " + prettyName(aComboboxID);
  };
}


/**
 * Select all invoker.
 */
function synthSelectAll(aNodeOrID, aCheckerOrEventSeq) {
  this.__proto__ = new synthAction(aNodeOrID, aCheckerOrEventSeq);

  this.invoke = function synthSelectAll_invoke() {
    if (this.DOMNode instanceof Components.interfaces.nsIDOMHTMLInputElement ||
        this.DOMNode instanceof Components.interfaces.nsIDOMXULTextBoxElement) {
      this.DOMNode.select();

    } else {
      window.getSelection().selectAllChildren(this.DOMNode);
    }
  };

  this.getID = function synthSelectAll_getID() {
    return aNodeOrID + " selectall";
  };
}

/**
 * Move the caret to the end of line.
 */
function moveToLineEnd(aID, aCaretOffset) {
  if (MAC) {
    this.__proto__ = new synthKey(aID, "VK_RIGHT", { metaKey: true },
                                  new caretMoveChecker(aCaretOffset, aID));
  } else {
    this.__proto__ = new synthEndKey(aID,
                                     new caretMoveChecker(aCaretOffset, aID));
  }

  this.getID = function moveToLineEnd_getID() {
    return "move to line end in " + prettyName(aID);
  };
}

/**
 * Move the caret to the end of previous line if any.
 */
function moveToPrevLineEnd(aID, aCaretOffset) {
  this.__proto__ = new synthAction(aID, new caretMoveChecker(aCaretOffset, aID));

  this.invoke = function moveToPrevLineEnd_invoke() {
    synthesizeKey("VK_UP", { });

    if (MAC)
      synthesizeKey("VK_RIGHT", { metaKey: true });
    else
      synthesizeKey("VK_END", { });
  };

  this.getID = function moveToPrevLineEnd_getID() {
    return "move to previous line end in " + prettyName(aID);
  };
}

/**
 * Move the caret to begining of the line.
 */
function moveToLineStart(aID, aCaretOffset) {
  if (MAC) {
    this.__proto__ = new synthKey(aID, "VK_LEFT", { metaKey: true },
                                  new caretMoveChecker(aCaretOffset, aID));
  } else {
    this.__proto__ = new synthHomeKey(aID,
                                      new caretMoveChecker(aCaretOffset, aID));
  }

  this.getID = function moveToLineEnd_getID() {
    return "move to line start in " + prettyName(aID);
  };
}

/**
 * Move the caret to begining of the text.
 */
function moveToTextStart(aID) {
  if (MAC) {
    this.__proto__ = new synthKey(aID, "VK_UP", { metaKey: true },
                                  new caretMoveChecker(0, aID));
  } else {
    this.__proto__ = new synthKey(aID, "VK_HOME", { ctrlKey: true },
                                  new caretMoveChecker(0, aID));
  }

  this.getID = function moveToTextStart_getID() {
    return "move to text start in " + prettyName(aID);
  };
}

/**
 * Move the caret in text accessible.
 */
function moveCaretToDOMPoint(aID, aDOMPointNodeID, aDOMPointOffset,
                             aExpectedOffset, aFocusTargetID,
                             aCheckFunc) {
  this.target = getAccessible(aID, [nsIAccessibleText]);
  this.DOMPointNode = getNode(aDOMPointNodeID);
  this.focus = aFocusTargetID ? getAccessible(aFocusTargetID) : null;
  this.focusNode = this.focus ? this.focus.DOMNode : null;

  this.invoke = function moveCaretToDOMPoint_invoke() {
    if (this.focusNode)
      this.focusNode.focus();

    var selection = this.DOMPointNode.ownerGlobal.getSelection();
    var selRange = selection.getRangeAt(0);
    selRange.setStart(this.DOMPointNode, aDOMPointOffset);
    selRange.collapse(true);

    selection.removeRange(selRange);
    selection.addRange(selRange);
  };

  this.getID = function moveCaretToDOMPoint_getID() {
   return "Set caret on " + prettyName(aID) + " at point: " +
     prettyName(aDOMPointNodeID) + " node with offset " + aDOMPointOffset;
  };

  this.finalCheck = function moveCaretToDOMPoint_finalCheck() {
    if (aCheckFunc)
      aCheckFunc.call();
  };

  this.eventSeq = [
    new caretMoveChecker(aExpectedOffset, this.target)
  ];

  if (this.focus)
    this.eventSeq.push(new asyncInvokerChecker(EVENT_FOCUS, this.focus));
}

/**
 * Set caret offset in text accessible.
 */
function setCaretOffset(aID, aOffset, aFocusTargetID) {
  this.target = getAccessible(aID, [nsIAccessibleText]);
  this.offset = aOffset == -1 ? this.target.characterCount : aOffset;
  this.focus = aFocusTargetID ? getAccessible(aFocusTargetID) : null;

  this.invoke = function setCaretOffset_invoke() {
    this.target.caretOffset = this.offset;
  };

  this.getID = function setCaretOffset_getID() {
   return "Set caretOffset on " + prettyName(aID) + " at " + this.offset;
  };

  this.eventSeq = [
    new caretMoveChecker(this.offset, this.target)
  ];

  if (this.focus)
    this.eventSeq.push(new asyncInvokerChecker(EVENT_FOCUS, this.focus));
}


// //////////////////////////////////////////////////////////////////////////////
// Event queue checkers

/**
 * Common invoker checker (see eventSeq of eventQueue).
 */
function invokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg, aIsAsync) {
  this.type = aEventType;
  this.async = aIsAsync;

  this.__defineGetter__("target", invokerChecker_targetGetter);
  this.__defineSetter__("target", invokerChecker_targetSetter);

  // implementation details
  function invokerChecker_targetGetter() {
    if (typeof this.mTarget == "function")
      return this.mTarget.call(null, this.mTargetFuncArg);
    if (typeof this.mTarget == "string")
      return getNode(this.mTarget);

    return this.mTarget;
  }

  function invokerChecker_targetSetter(aValue) {
    this.mTarget = aValue;
    return this.mTarget;
  }

  this.__defineGetter__("targetDescr", invokerChecker_targetDescrGetter);

  function invokerChecker_targetDescrGetter() {
    if (typeof this.mTarget == "function")
      return this.mTarget.name + ", arg: " + this.mTargetFuncArg;

    return prettyName(this.mTarget);
  }

  this.mTarget = aTargetOrFunc;
  this.mTargetFuncArg = aTargetFuncArg;
}

/**
 * event checker that forces preceeding async events to happen before this
 * checker.
 */
function orderChecker() {
  // XXX it doesn't actually work to inherit from invokerChecker, but maybe we
  // should fix that?
  //  this.__proto__ = new invokerChecker(null, null, null, false);
}

/**
 * Generic invoker checker for todo events.
 */
function todo_invokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new invokerChecker(aEventType, aTargetOrFunc,
                                      aTargetFuncArg, true);
  this.todo = true;
}

/**
 * Generic invoker checker for unexpected events.
 */
function unexpectedInvokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new invokerChecker(aEventType, aTargetOrFunc,
                                      aTargetFuncArg, true);

  this.unexpected = true;
}

/**
 * Common invoker checker for async events.
 */
function asyncInvokerChecker(aEventType, aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new invokerChecker(aEventType, aTargetOrFunc,
                                      aTargetFuncArg, true);
}

function focusChecker(aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new invokerChecker(EVENT_FOCUS, aTargetOrFunc,
                                      aTargetFuncArg, false);

  this.unique = true; // focus event must be unique for invoker action

  this.check = function focusChecker_check(aEvent) {
    testStates(aEvent.accessible, STATE_FOCUSED);
  };
}

function nofocusChecker(aID) {
  this.__proto__ = new focusChecker(aID);
  this.unexpected = true;
}

/**
 * Text inserted/removed events checker.
 * @param aFromUser  [in, optional] kNotFromUserInput or kFromUserInput
 */
function textChangeChecker(aID, aStart, aEnd, aTextOrFunc, aIsInserted, aFromUser, aAsync) {
  this.target = getNode(aID);
  this.type = aIsInserted ? EVENT_TEXT_INSERTED : EVENT_TEXT_REMOVED;
  this.startOffset = aStart;
  this.endOffset = aEnd;
  this.textOrFunc = aTextOrFunc;
  this.async = aAsync;

  this.match = function stextChangeChecker_match(aEvent) {
    if (!(aEvent instanceof nsIAccessibleTextChangeEvent) ||
        aEvent.accessible !== getAccessible(this.target)) {
      return false;
    }

    let tcEvent = aEvent.QueryInterface(nsIAccessibleTextChangeEvent);
    let modifiedText = (typeof this.textOrFunc === "function") ?
      this.textOrFunc() : this.textOrFunc;
    return modifiedText === tcEvent.modifiedText;
  };

  this.check = function textChangeChecker_check(aEvent) {
    aEvent.QueryInterface(nsIAccessibleTextChangeEvent);

    var modifiedText = (typeof this.textOrFunc == "function") ?
      this.textOrFunc() : this.textOrFunc;
    var modifiedTextLen =
      (this.endOffset == -1) ? modifiedText.length : aEnd - aStart;

    is(aEvent.start, this.startOffset,
       "Wrong start offset for " + prettyName(aID));
    is(aEvent.length, modifiedTextLen, "Wrong length for " + prettyName(aID));
    var changeInfo = (aIsInserted ? "inserted" : "removed");
    is(aEvent.isInserted, aIsInserted,
       "Text was " + changeInfo + " for " + prettyName(aID));
    is(aEvent.modifiedText, modifiedText,
       "Wrong " + changeInfo + " text for " + prettyName(aID));
    if (typeof aFromUser != "undefined")
      is(aEvent.isFromUserInput, aFromUser,
         "wrong value of isFromUserInput() for " + prettyName(aID));
  };
}

/**
 * Caret move events checker.
 */
function caretMoveChecker(aCaretOffset, aTargetOrFunc, aTargetFuncArg,
                          aIsAsync) {
  this.__proto__ = new invokerChecker(EVENT_TEXT_CARET_MOVED,
                                      aTargetOrFunc, aTargetFuncArg, aIsAsync);

  this.check = function caretMoveChecker_check(aEvent) {
    is(aEvent.QueryInterface(nsIAccessibleCaretMoveEvent).caretOffset,
       aCaretOffset,
       "Wrong caret offset for " + prettyName(aEvent.accessible));
  };
}

function asyncCaretMoveChecker(aCaretOffset, aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new caretMoveChecker(aCaretOffset, aTargetOrFunc,
                                        aTargetFuncArg, true);
}

/**
 * Text selection change checker.
 */
function textSelectionChecker(aID, aStartOffset, aEndOffset) {
  this.__proto__ = new invokerChecker(EVENT_TEXT_SELECTION_CHANGED, aID);

  this.check = function textSelectionChecker_check(aEvent) {
    if (aStartOffset == aEndOffset) {
      ok(true, "Collapsed selection triggered text selection change event.");
    } else {
      testTextGetSelection(aID, aStartOffset, aEndOffset, 0);
    }
  };
}

/**
 * Object attribute changed checker
 */
function objAttrChangedChecker(aID, aAttr) {
  this.__proto__ = new invokerChecker(EVENT_OBJECT_ATTRIBUTE_CHANGED, aID);

  this.check = function objAttrChangedChecker_check(aEvent) {
    var event = null;
    try {
      var event = aEvent.QueryInterface(
        nsIAccessibleObjectAttributeChangedEvent);
    } catch (e) {
      ok(false, "Object attribute changed event was expected");
    }

    if (!event) {
      return;
    }

    is(event.changedAttribute, aAttr,
      "Wrong attribute name of the object attribute changed event.");
  };

  this.match = function objAttrChangedChecker_match(aEvent) {
    if (aEvent instanceof nsIAccessibleObjectAttributeChangedEvent) {
      var scEvent = aEvent.QueryInterface(
        nsIAccessibleObjectAttributeChangedEvent);
      return (aEvent.accessible == getAccessible(this.target)) &&
        (scEvent.changedAttribute == aAttr);
    }
    return false;
  };
}

/**
 * State change checker.
 */
function stateChangeChecker(aState, aIsExtraState, aIsEnabled,
                            aTargetOrFunc, aTargetFuncArg, aIsAsync,
                            aSkipCurrentStateCheck) {
  this.__proto__ = new invokerChecker(EVENT_STATE_CHANGE, aTargetOrFunc,
                                      aTargetFuncArg, aIsAsync);

  this.check = function stateChangeChecker_check(aEvent) {
    var event = null;
    try {
      var event = aEvent.QueryInterface(nsIAccessibleStateChangeEvent);
    } catch (e) {
      ok(false, "State change event was expected");
    }

    if (!event)
      return;

    is(event.isExtraState, aIsExtraState,
       "Wrong extra state bit of the statechange event.");
    isState(event.state, aState, aIsExtraState,
            "Wrong state of the statechange event.");
    is(event.isEnabled, aIsEnabled,
      "Wrong state of statechange event state");

    if (aSkipCurrentStateCheck) {
      todo(false, "State checking was skipped!");
      return;
    }

    var state = aIsEnabled ? (aIsExtraState ? 0 : aState) : 0;
    var extraState = aIsEnabled ? (aIsExtraState ? aState : 0) : 0;
    var unxpdState = aIsEnabled ? 0 : (aIsExtraState ? 0 : aState);
    var unxpdExtraState = aIsEnabled ? 0 : (aIsExtraState ? aState : 0);
    testStates(event.accessible, state, extraState, unxpdState, unxpdExtraState);
  };

  this.match = function stateChangeChecker_match(aEvent) {
    if (aEvent instanceof nsIAccessibleStateChangeEvent) {
      var scEvent = aEvent.QueryInterface(nsIAccessibleStateChangeEvent);
      return (aEvent.accessible == getAccessible(this.target)) &&
        (scEvent.state == aState);
    }
    return false;
  };
}

function asyncStateChangeChecker(aState, aIsExtraState, aIsEnabled,
                                 aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new stateChangeChecker(aState, aIsExtraState, aIsEnabled,
                                          aTargetOrFunc, aTargetFuncArg, true);
}

/**
 * Expanded state change checker.
 */
function expandedStateChecker(aIsEnabled, aTargetOrFunc, aTargetFuncArg) {
  this.__proto__ = new invokerChecker(EVENT_STATE_CHANGE, aTargetOrFunc,
                                      aTargetFuncArg);

  this.check = function expandedStateChecker_check(aEvent) {
    var event = null;
    try {
      var event = aEvent.QueryInterface(nsIAccessibleStateChangeEvent);
    } catch (e) {
      ok(false, "State change event was expected");
    }

    if (!event)
      return;

    is(event.state, STATE_EXPANDED, "Wrong state of the statechange event.");
    is(event.isExtraState, false,
       "Wrong extra state bit of the statechange event.");
    is(event.isEnabled, aIsEnabled,
      "Wrong state of statechange event state");

    testStates(event.accessible,
               (aIsEnabled ? STATE_EXPANDED : STATE_COLLAPSED));
  };
}

// //////////////////////////////////////////////////////////////////////////////
// Event sequances (array of predefined checkers)

/**
 * Event seq for single selection change.
 */
function selChangeSeq(aUnselectedID, aSelectedID) {
  if (!aUnselectedID) {
    return [
      new stateChangeChecker(STATE_SELECTED, false, true, aSelectedID),
      new invokerChecker(EVENT_SELECTION, aSelectedID)
    ];
  }

  // Return two possible scenarios: depending on widget type when selection is
  // moved the the order of items that get selected and unselected may vary.
  return [
    [
      new stateChangeChecker(STATE_SELECTED, false, false, aUnselectedID),
      new stateChangeChecker(STATE_SELECTED, false, true, aSelectedID),
      new invokerChecker(EVENT_SELECTION, aSelectedID)
    ],
    [
      new stateChangeChecker(STATE_SELECTED, false, true, aSelectedID),
      new stateChangeChecker(STATE_SELECTED, false, false, aUnselectedID),
      new invokerChecker(EVENT_SELECTION, aSelectedID)
    ]
  ];
}

/**
 * Event seq for item removed form the selection.
 */
function selRemoveSeq(aUnselectedID) {
  return [
    new stateChangeChecker(STATE_SELECTED, false, false, aUnselectedID),
    new invokerChecker(EVENT_SELECTION_REMOVE, aUnselectedID)
  ];
}

/**
 * Event seq for item added to the selection.
 */
function selAddSeq(aSelectedID) {
  return [
    new stateChangeChecker(STATE_SELECTED, false, true, aSelectedID),
    new invokerChecker(EVENT_SELECTION_ADD, aSelectedID)
  ];
}

// //////////////////////////////////////////////////////////////////////////////
// Private implementation details.
// //////////////////////////////////////////////////////////////////////////////


// //////////////////////////////////////////////////////////////////////////////
// General

var gA11yEventListeners = {};
var gA11yEventApplicantsCount = 0;

var gA11yEventObserver =
{
  observe: function observe(aSubject, aTopic, aData) {
    if (aTopic != "accessible-event")
      return;

    var event;
    try {
      event = aSubject.QueryInterface(nsIAccessibleEvent);
    } catch (ex) {
      // After a test is aborted (i.e. timed out by the harness), this exception is soon triggered.
      // Remove the leftover observer, otherwise it "leaks" to all the following tests.
      Services.obs.removeObserver(this, "accessible-event");
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

        if (event instanceof nsIAccessibleStateChangeEvent) {
          var stateStr = statesToString(event.isExtraState ? 0 : event.state,
                                        event.isExtraState ? event.state : 0);
          info += ", state: " + stateStr + ", is enabled: " + event.isEnabled;

        } else if (event instanceof nsIAccessibleTextChangeEvent) {
          info += ", start: " + event.start + ", length: " + event.length +
            ", " + (event.isInserted ? "inserted" : "removed") +
            " text: " + event.modifiedText;
        }

        info += ". Target: " + prettyName(event.accessible);

        if (listenersArray)
          info += ". Listeners count: " + listenersArray.length;

        if (gLogger.hasFeature("parentchain:" + type)) {
          info += "\nParent chain:\n";
          var acc = event.accessible;
          while (acc) {
            info += "  " + prettyName(acc) + "\n";
            acc = acc.parent;
          }
        }

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

function listenA11yEvents(aStartToListen) {
  if (aStartToListen) {
    // Add observer when adding the first applicant only.
    if (!(gA11yEventApplicantsCount++))
      Services.obs.addObserver(gA11yEventObserver, "accessible-event");
  } else {
    // Remove observer when there are no more applicants only.
    // '< 0' case should not happen, but just in case: removeObserver() will throw.
    // eslint-disable-next-line no-lonely-if
    if (--gA11yEventApplicantsCount <= 0)
      Services.obs.removeObserver(gA11yEventObserver, "accessible-event");
  }
}

function addA11yEventListener(aEventType, aEventHandler) {
  if (!(aEventType in gA11yEventListeners))
    gA11yEventListeners[aEventType] = [];

  var listenersArray = gA11yEventListeners[aEventType];
  var index = listenersArray.indexOf(aEventHandler);
  if (index == -1)
    listenersArray.push(aEventHandler);
}

function removeA11yEventListener(aEventType, aEventHandler) {
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
  isEnabled: function debugOutput_isEnabled() {
    return gA11yEventDumpID || gA11yEventDumpToConsole ||
      gA11yEventDumpToAppConsole;
  },

  /**
   * Dump information into DOM and console if applicable.
   */
  log: function logger_log(aMsg) {
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
  logToDOM: function logger_logToDOM(aMsg, aHasIndent, aPreEmphText) {
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
  logToConsole: function logger_logToConsole(aMsg) {
    if (gA11yEventDumpToConsole)
      dump("\n" + aMsg + "\n");
  },

  /**
   * Log message to error console.
   */
  logToAppConsole: function logger_logToAppConsole(aMsg) {
    if (gA11yEventDumpToAppConsole)
      Services.console.logStringMessage("events: " + aMsg);
  },

  /**
   * Return true if logging feature is enabled.
   */
  hasFeature: function logger_hasFeature(aFeature) {
    var startIdx = gA11yEventDumpFeature.indexOf(aFeature);
    if (startIdx == -1)
      return false;

    var endIdx = startIdx + aFeature.length;
    return endIdx == gA11yEventDumpFeature.length ||
      gA11yEventDumpFeature[endIdx] == ";";
  }
};


// //////////////////////////////////////////////////////////////////////////////
// Sequence

/**
 * Base class of sequence item.
 */
function sequenceItem(aProcessor, aEventType, aTarget, aItemID) {
  // private

  this.startProcess = function sequenceItem_startProcess() {
    this.queue.invoke();
  };

  this.queue = new eventQueue();
  this.queue.onFinish = function() {
    aProcessor.onProcessed();
    return DO_NOT_FINISH_TEST;
  };

  var invoker = {
    invoke: function invoker_invoke() {
      return aProcessor.process();
    },
    getID: function invoker_getID() {
      return aItemID;
    },
    eventSeq: [ new invokerChecker(aEventType, aTarget) ]
  };

  this.queue.push(invoker);
}

// //////////////////////////////////////////////////////////////////////////////
// Event queue invokers

/**
 * Invoker base class for prepare an action.
 */
function synthAction(aNodeOrID, aEventsObj) {
  this.DOMNode = getNode(aNodeOrID);

  if (aEventsObj) {
    var scenarios = null;
    if (aEventsObj instanceof Array) {
      if (aEventsObj[0] instanceof Array)
        scenarios = aEventsObj; // scenarios
      else
        scenarios = [ aEventsObj ]; // event sequance
    } else {
      scenarios = [ [ aEventsObj ] ]; // a single checker object
    }

    for (var i = 0; i < scenarios.length; i++)
      defineScenario(this, scenarios[i]);
  }

  this.getID = function synthAction_getID() { return prettyName(aNodeOrID) + " action"; };
}
