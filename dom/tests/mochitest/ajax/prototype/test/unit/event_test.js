var documentLoaded = document.loaded;

new Test.Unit.Runner({
  
  // test firing an event and observing it on the element it's fired from
  testCustomEventFiring: function() {
    var span = $("span"), fired = false, observer = function(event) {
      this.assertEqual(span, event.element());
      this.assertEqual(1, event.memo.index);
      fired = true;
    }.bind(this);
    
    span.observe("test:somethingHappened", observer);
    span.fire("test:somethingHappened", { index: 1 });
    this.assert(fired);
    
    fired = false;
    span.fire("test:somethingElseHappened");
    this.assert(!fired);
    
    span.stopObserving("test:somethingHappened", observer);
    span.fire("test:somethingHappened");
    this.assert(!fired);
  },
  
  // test firing an event and observing it on a containing element
  testCustomEventBubbling: function() {
    var span = $("span"), outer = $("outer"), fired = false, observer = function(event) {
      this.assertEqual(span, event.element());
      fired = true;
    }.bind(this);
    
    outer.observe("test:somethingHappened", observer);
    span.fire("test:somethingHappened");
    this.assert(fired);
    
    fired = false;
    span.fire("test:somethingElseHappened");
    this.assert(!fired);
    
    outer.stopObserving("test:somethingHappened", observer);
    span.fire("test:somethingHappened");
    this.assert(!fired);
  },
  
  testCustomEventCanceling: function() {
    var span = $("span"), outer = $("outer"), inner = $("inner");
    var fired = false, stopped = false;

    function outerObserver(event) {
      fired = span == event.element();
    }
    
    function innerObserver(event) {
      event.stop();
      stopped = true;
    }
    
    inner.observe("test:somethingHappened", innerObserver);
    outer.observe("test:somethingHappened", outerObserver);
    span.fire("test:somethingHappened");
    this.assert(stopped);
    this.assert(!fired);
    
    fired = stopped = false;
    inner.stopObserving("test:somethingHappened", innerObserver);
    span.fire("test:somethingHappened");
    this.assert(!stopped);
    this.assert(fired);
    
    outer.stopObserving("test:somethingHappened", outerObserver);
  },
  
  testEventObjectIsExtended: function() { 
    var span = $("span"), event, observedEvent, observer = function(e) { observedEvent = e };
    span.observe("test:somethingHappened", observer);
    event = span.fire("test:somethingHappened");
    this.assertEqual(event, observedEvent);
    this.assertEqual(Event.Methods.stop.methodize(), event.stop);
    span.stopObserving("test:somethingHappened", observer);
    
    event = span.fire("test:somethingHappenedButNoOneIsListening");
    this.assertEqual(Event.Methods.stop.methodize(), event.stop);
  },
  
  testEventObserversAreBoundToTheObservedElement: function() {
    var span = $("span"), target, observer = function() { target = this };
    
    span.observe("test:somethingHappened", observer);
    span.fire("test:somethingHappened");
    span.stopObserving("test:somethingHappened", observer);
    this.assertEqual(span, target);
    target = null;
    
    var outer = $("outer");
    outer.observe("test:somethingHappened", observer);
    span.fire("test:somethingHappened");
    outer.stopObserving("test:somethingHappened", observer);
    this.assertEqual(outer, target);
  },
  
  testMultipleCustomEventObserversWithTheSameHandler: function() {
    var span = $("span"), count = 0, observer = function() { count++ };
    
    span.observe("test:somethingHappened", observer);
    span.observe("test:somethingElseHappened", observer);
    span.fire("test:somethingHappened");
    this.assertEqual(1, count);
    span.fire("test:somethingElseHappened");
    this.assertEqual(2, count);
    span.stopObserving("test:somethingHappened", observer); 
    span.stopObserving("test:somethingElseHappened", observer);   
  },
  
  testStopObservingWithoutArguments: function() {
    var span = $("span"), count = 0, observer = function() { count++ };
    
    span.observe("test:somethingHappened", observer);
    span.observe("test:somethingElseHappened", observer);
    span.stopObserving();
    span.fire("test:somethingHappened");
    this.assertEqual(0, count);
    span.fire("test:somethingElseHappened");
    this.assertEqual(0, count);

    this.assertEqual(window, Event.stopObserving(window));
    
    // test element with no observers
    this.assertNothingRaised(function() { $(document.body).stopObserving() });
  },
  
  testStopObservingWithoutHandlerArgument: function() {
    var span = $("span"), count = 0, observer = function() { count++ };
    
    span.observe("test:somethingHappened", observer);
    span.observe("test:somethingElseHappened", observer);
    span.stopObserving("test:somethingHappened");
    span.fire("test:somethingHappened");
    this.assertEqual(0, count);
    span.fire("test:somethingElseHappened");
    this.assertEqual(1, count);
    span.stopObserving("test:somethingElseHappened");
    span.fire("test:somethingElseHappened");
    this.assertEqual(1, count);
    
    // test element with no observers
    this.assertNothingRaised(function() { $(document.body).stopObserving("test:somethingHappened") });
  },
  
  testStopObservingRemovesHandlerFromCache: function() {
    var span = $("span"), observer = function() { }, eventID;
    
    span.observe("test:somethingHappened", observer);
    eventID = span._prototypeEventID;
    
    this.assert(Event.cache[eventID]);
    this.assert(Object.isArray(Event.cache[eventID]["test:somethingHappened"]));
    this.assertEqual(1, Event.cache[eventID]["test:somethingHappened"].length);
    
    span.stopObserving("test:somethingHappened", observer);
    this.assert(Event.cache[eventID]);
    this.assert(Object.isArray(Event.cache[eventID]["test:somethingHappened"]));
    this.assertEqual(0, Event.cache[eventID]["test:somethingHappened"].length);
  },
  
  testObserveAndStopObservingAreChainable: function() {
    var span = $("span"), observer = function() { };

    this.assertEqual(span, span.observe("test:somethingHappened", observer));
    this.assertEqual(span, span.stopObserving("test:somethingHappened", observer));

    span.observe("test:somethingHappened", observer);
    this.assertEqual(span, span.stopObserving("test:somethingHappened"));

    span.observe("test:somethingHappened", observer);
    this.assertEqual(span, span.stopObserving());
    this.assertEqual(span, span.stopObserving()); // assert it again, after there are no observers

    span.observe("test:somethingHappened", observer);
    this.assertEqual(span, span.observe("test:somethingHappened", observer)); // try to reuse the same observer
    span.stopObserving();
  },

  testObserveInsideHandlers: function() {
    var fired = false, observer = function(event) {
      fired = true;
    };
    
    document.observe("test:somethingHappened", function() {
      document.observe("test:somethingHappened", observer);
    });
    
    document.fire("test:somethingHappened");
    this.assert(!fired);
    
    document.fire("test:somethingHappened");
    this.assert(fired);
    document.stopObserving("test:somethingHappened");
  },

  testStopObservingInsideHandlers: function() {
    var fired = false, observer = function(event) {
      fired = true;
    };
    
    document.observe("test:somethingHappened", observer);
    document.observe("test:somethingHappened", function() {
      document.stopObserving("test:somethingHappened", observer);
    });
    
    document.fire("test:somethingHappened");
    this.assert(fired);
    
    fired = false;
    document.fire("test:somethingHappened");
    document.stopObserving("test:somethingHappened");
    this.assert(!fired);
  },

  testDocumentLoaded: function() {
    this.assert(!documentLoaded);
    this.assert(document.loaded);
  },
  
  testDocumentContentLoadedEventFiresBeforeWindowLoad: function() {
    this.assert(eventResults.contentLoaded, "contentLoaded");
    this.assert(eventResults.contentLoaded.endOfDocument, "contentLoaded.endOfDocument");
    this.assert(!eventResults.contentLoaded.windowLoad, "!contentLoaded.windowLoad");
    this.assert(eventResults.windowLoad, "windowLoad");
    this.assert(eventResults.windowLoad.endOfDocument, "windowLoad.endOfDocument");
    this.assert(eventResults.windowLoad.contentLoaded, "windowLoad.contentLoaded");
  },
  
  testEventStopped: function() {
    var span = $("span"), event;

    span.observe("test:somethingHappened", function() { });
    event = span.fire("test:somethingHappened");
    this.assert(!event.stopped, "event.stopped should be false with an empty observer");
    span.stopObserving("test:somethingHappened");
    
    span.observe("test:somethingHappened", function(e) { e.stop() });
    event = span.fire("test:somethingHappened");
    this.assert(event.stopped, "event.stopped should be true for an observer that calls stop");
    span.stopObserving("test:somethingHappened");
  },

  testEventFindElement: function() {
    var span = $("span"), event;
    event = span.fire("test:somethingHappened");
    this.assertElementMatches(event.findElement(), 'span#span');
    this.assertElementMatches(event.findElement('span'), 'span#span');
    this.assertElementMatches(event.findElement('p'), 'p#inner');
    this.assertEqual(null, event.findElement('div.does_not_exist'));
    this.assertElementMatches(event.findElement('.does_not_exist, span'), 'span#span');
  },
  
  testEventIDDuplication: function() {
    $('container').down().observe("test:somethingHappened", Prototype.emptyFunction);
    $('container').innerHTML += $('container').innerHTML;
    this.assertUndefined($('container').down(1)._prototypeEventID);
  },
  
  testHandlerCallsPreserveOrder: function() {
    var span = $("span"), order = [ ];
    span.observe("test:somethingHappened", function(e) { order.push('first') }); 
    span.observe("test:somethingHappened", function(e) { order.push('second') }); 
    span.fire("test:somethingHappened"); 
    this.assertEnumEqual(['first', 'second'], order); 
    span.stopObserving("test:somethingHappened"); 
  }
});

document.observe("dom:loaded", function(event) {
  eventResults.contentLoaded = {
    endOfDocument: eventResults.endOfDocument,
    windowLoad:    eventResults.windowLoad
  };
});

Event.observe(window, "load", function(event) {
  eventResults.windowLoad = {
    endOfDocument: eventResults.endOfDocument,
    contentLoaded: eventResults.contentLoaded
  };
});
