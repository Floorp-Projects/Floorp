// Helper class to check DOM MutationEvents
//
// Usage:
//
// * Create a new event checker:
//     var eventChecker = new MutationEventChecker;
// * Set the attribute to watch
//     eventChecker.watchAttr(<DOM element>, "<attribute name>");
// * Set the events to expect (0..n)
//     eventChecker.expect("add", "modify");
//     OR
//     eventChecker.expect("add modify");
//     OR
//     eventChecker.expect(MutationEvent.ADDITION, MutationEvent.MODIFICATION);
//
//  An empty string or empty set of arguments is also fine as a way of checking
//  that all expected events have been received and indicating no events are
//  expected from the following code, e.g.
//
//    eventChecker.expect("");
//    // changes that are not expected to generate events
//    eventChecker.expect("modify");
//    // change that is expected to generate an event
//    ...
//
// * Either finish listening or set the next attribute to watch
//     eventChecker.finish();
//     eventChecker.watchAttr(element, "nextAttribute");
//
//   In either case a check is performed that all expected events have been
//   received.
//
// * Event checking can be temporarily disabled with ignoreEvents(). The next
//   call to expect() will cause it to resume.

function MutationEventChecker() {
  this.expectedEvents = [];

  this.watchAttr = function(element, attr) {
    if (this.attr) {
      this.finish();
    }

    this.expectedEvents = [];
    this.element = element;
    this.attr = attr;
    this.oldValue = element.getAttribute(attr);
    this.giveUp = false;
    this.ignore = false;

    this.element.addEventListener("DOMAttrModified", this._listener);
  };

  this.expect = function() {
    if (this.giveUp) {
      return;
    }

    ok(
      !this.expectedEvents.length,
      "Expecting new events for " +
        this.attr +
        " but the following previously expected events have still not been " +
        "received: " +
        this._stillExpecting()
    );
    if (this.expectedEvents.length) {
      this.giveUp = true;
      return;
    }

    this.ignore = false;

    if (!arguments.length || (arguments.length == 1 && arguments[0] == "")) {
      return;
    }

    // Turn arguments object into an array
    var args = Array.prototype.slice.call(arguments);
    // Check for whitespace separated keywords
    if (
      args.length == 1 &&
      typeof args[0] === "string" &&
      args[0].indexOf(" ") > 0
    ) {
      args = args[0].split(" ");
    }
    // Convert strings to event Ids
    this.expectedEvents = args.map(this._argToEventId);
  };

  // Temporarily disable event checking
  this.ignoreEvents = function() {
    // Check all events have been received
    ok(
      this.giveUp || !this.expectedEvents.length,
      "Going to ignore subsequent events on " +
        this.attr +
        " attribute, but we're still expecting the following events: " +
        this._stillExpecting()
    );

    this.ignore = true;
  };

  this.finish = function() {
    // Check all events have been received
    ok(
      this.giveUp || !this.expectedEvents.length,
      "Finishing listening to " +
        this.attr +
        " attribute, but we're still expecting the following events: " +
        this._stillExpecting()
    );

    this.element.removeEventListener("DOMAttrModified", this._listener);
    this.attr = "";
  };

  this._receiveEvent = function(e) {
    if (this.giveUp || this.ignore) {
      this.oldValue = e.newValue;
      return;
    }

    // Make sure we're expecting something at all
    if (!this.expectedEvents.length) {
      ok(
        false,
        "Unexpected " +
          this._eventToName(e.attrChange) +
          " event when none expected on " +
          this.attr +
          " attribute."
      );
      return;
    }

    var expectedEvent = this.expectedEvents.shift();

    // Make sure we got the event we expected
    if (e.attrChange != expectedEvent) {
      ok(
        false,
        "Unexpected " +
          this._eventToName(e.attrChange) +
          " on " +
          this.attr +
          " attribute. Expected " +
          this._eventToName(expectedEvent) +
          " (followed by: " +
          this._stillExpecting() +
          ")"
      );
      // If we get events out of sequence, it doesn't make sense to do any
      // further testing since we don't really know what to expect
      this.giveUp = true;
      return;
    }

    // Common param checking
    is(
      e.target,
      this.element,
      "Unexpected node for mutation event on " + this.attr + " attribute"
    );
    is(e.attrName, this.attr, "Unexpected attribute name for mutation event");

    // Don't bother testing e.relatedNode since Attr nodes are on the way
    // out anyway (but then, so are mutation events...)

    // Event-specific checking
    if (e.attrChange == MutationEvent.MODIFICATION) {
      ok(
        this.element.hasAttribute(this.attr),
        "Attribute not set after modification"
      );
      is(
        e.prevValue,
        this.oldValue,
        "Unexpected old value for modification to " + this.attr + " attribute"
      );
      isnot(
        e.newValue,
        this.oldValue,
        "Unexpected new value for modification to " + this.attr + " attribute"
      );
    } else if (e.attrChange == MutationEvent.REMOVAL) {
      ok(!this.element.hasAttribute(this.attr), "Attribute set after removal");
      is(
        e.prevValue,
        this.oldValue,
        "Unexpected old value for removal of " + this.attr + " attribute"
      );
      // DOM 3 Events doesn't say what value newValue will be for a removal
      // event but generally empty strings are used for other events when an
      // attribute isn't relevant
      ok(
        e.newValue === "",
        "Unexpected new value for removal of " + this.attr + " attribute"
      );
    } else if (e.attrChange == MutationEvent.ADDITION) {
      ok(
        this.element.hasAttribute(this.attr),
        "Attribute not set after addition"
      );
      // DOM 3 Events doesn't say what value prevValue will be for an addition
      // event but generally empty strings are used for other events when an
      // attribute isn't relevant
      ok(
        e.prevValue === "",
        "Unexpected old value for addition of " + this.attr + " attribute"
      );
      ok(
        typeof e.newValue == "string" && e.newValue !== "",
        "Unexpected new value for addition of " + this.attr + " attribute"
      );
    } else {
      ok(false, "Unexpected mutation event type: " + e.attrChange);
      this.giveUp = true;
    }
    this.oldValue = e.newValue;
  };
  this._listener = this._receiveEvent.bind(this);

  this._stillExpecting = function() {
    if (!this.expectedEvents.length) {
      return "(nothing)";
    }
    var eventNames = [];
    for (var i = 0; i < this.expectedEvents.length; i++) {
      eventNames.push(this._eventToName(this.expectedEvents[i]));
    }
    return eventNames.join(", ");
  };

  this._eventToName = function(evtId) {
    switch (evtId) {
      case MutationEvent.MODIFICATION:
        return "modification";
      case MutationEvent.ADDITION:
        return "addition";
      case MutationEvent.REMOVAL:
        return "removal";
    }
    return "Unknown MutationEvent Type";
  };

  this._argToEventId = function(arg) {
    if (typeof arg === "number") {
      return arg;
    }

    if (typeof arg !== "string") {
      ok(false, "Unexpected event type: " + arg);
      return 0;
    }

    switch (arg.toLowerCase()) {
      case "mod":
      case "modify":
      case "modification":
        return MutationEvent.MODIFICATION;

      case "add":
      case "addition":
        return MutationEvent.ADDITION;

      case "removal":
      case "remove":
        return MutationEvent.REMOVAL;

      default:
        ok(false, "Unexpected event name: " + arg);
        return 0;
    }
  };
}
