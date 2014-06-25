/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["StateMachine"];

const DEBUG = false;

this.StateMachine = function(aDebugTag) {
  function debug(aMsg) {
    dump('-------------- StateMachine:' + aDebugTag + ': ' + aMsg);
  }

  var sm = {};

  var _initialState;
  var _curState;
  var _prevState;
  var _paused;
  var _eventQueue = [];
  var _deferredEventQueue = [];
  var _defaultEventHandler;

  // Public interfaces.

  sm.setDefaultEventHandler = function(aDefaultEventHandler) {
    _defaultEventHandler = aDefaultEventHandler;
  };

  sm.start = function(aInitialState) {
    _initialState = aInitialState;
    sm.gotoState(_initialState);
  };

  sm.sendEvent = function (aEvent) {
    if (!_initialState) {
      if (DEBUG) {
        debug('StateMachine is not running. Call StateMachine.start() first.');
      }
      return;
    }
    _eventQueue.push(aEvent);
    asyncCall(handleFirstEvent);
  };

  sm.getPreviousState = function() {
    return _prevState;
  };

  sm.getCurrentState = function() {
    return _curState;
  };

  // State object maker.
  // @param aName string for this state's name.
  // @param aDelegate object:
  //    .handleEvent: required.
  //    .enter: called before entering this state (optional).
  //    .exit: called before exiting this state (optional).
  sm.makeState = function (aName, aDelegate) {
    if (!aDelegate.handleEvent) {
      throw "handleEvent is a required delegate function.";
    }
    var nop = function() {};
    return {
      name: aName,
      enter: (aDelegate.enter || nop),
      exit: (aDelegate.exit || nop),
      handleEvent: aDelegate.handleEvent
    };
  };

  sm.deferEvent = function (aEvent) {
    // The definition of a 'deferred event' is:
    //     We are not able to handle this event now but after receiving
    //     certain event or entering a new state, we might be able to handle
    //     it. For example, we couldn't handle CONNECT_EVENT in the
    //     diconnecting state. But once we finish doing "disconnecting", we
    //     could then handle CONNECT_EVENT!
    //
    // So, the deferred event may be handled in the following cases:
    //     1. Once we entered a new state.
    //     2. Once we handled a regular event.
    if (DEBUG) {
      debug('Deferring event: ' + JSON.stringify(aEvent));
    }
    _deferredEventQueue.push(aEvent);
  };

  // Goto the new state. If the current state is null, the exit
  // function won't be called.
  sm.gotoState = function (aNewState) {
    if (_curState) {
      if (DEBUG) {
        debug("exiting state: " + _curState.name);
      }
      _curState.exit();
    }

    _prevState = _curState;
    _curState = aNewState;

    if (DEBUG) {
      debug("entering state: " + _curState.name);
    }
    _curState.enter();

    // We are in the new state now. We got a chance to handle the
    // deferred events.
    handleDeferredEvents();

    sm.resume();
  };

  // No incoming event will be handled after you call pause().
  // (But they will be queued.)
  sm.pause = function() {
    _paused = true;
  };

  // Continue to handle incoming events.
  sm.resume = function() {
    _paused = false;
    asyncCall(handleFirstEvent);
  };

  //----------------------------------------------------------
  // Private stuff
  //----------------------------------------------------------

  function asyncCall(f) {
    Services.tm.currentThread.dispatch(f, Ci.nsIThread.DISPATCH_NORMAL);
  }

  function handleFirstEvent() {
    var hadDeferredEvents;

    if (0 === _eventQueue.length) {
      return;
    }

    if (_paused) {
      return; // The state machine is paused now.
    }

    hadDeferredEvents = _deferredEventQueue.length > 0;

    handleOneEvent(_eventQueue.shift()); // The handler may defer this event.

    // We've handled one event. If we had deferred events before, now is
    // a good chance to handle them.
    if (hadDeferredEvents) {
      handleDeferredEvents();
    }

    // Continue to handle the next regular event.
    handleFirstEvent();
  }

  function handleDeferredEvents() {
    if (_deferredEventQueue.length && DEBUG) {
      debug('Handle deferred events: ' + _deferredEventQueue.length);
    }
    for (let i = 0; i < _deferredEventQueue.length; i++) {
      handleOneEvent(_deferredEventQueue.shift());
    }
  }

  function handleOneEvent(aEvent)
  {
    if (DEBUG) {
      debug('Handling event: ' + JSON.stringify(aEvent));
    }

    var handled = _curState.handleEvent(aEvent);

    if (undefined === handled) {
      throw "handleEvent returns undefined: " + _curState.name;
    }
    if (!handled) {
      // Event is not handled in the current state. Try handleEventCommon().
      handled = (_defaultEventHandler ? _defaultEventHandler(aEvent) : handled);
    }
    if (undefined === handled) {
      throw "handleEventCommon returns undefined: " + _curState.name;
    }
    if (!handled) {
      if (DEBUG) {
        debug('!!!!!!!!! FIXME !!!!!!!!! Event not handled: ' + JSON.stringify(aEvent));
      }
    }

    return handled;
  }

  return sm;
};
