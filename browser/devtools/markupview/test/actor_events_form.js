/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test actor is used for testing the addition of custom form data
// on NodeActor. Custom form property is set when 'form' event is sent
// by NodeActor actor (see 'onNodeActorForm' method).

const Events = require("sdk/event/core");
const {ActorClass, Actor, FrontClass, Front, method} =
  require("devtools/server/protocol");

const {Cu} = require("chrome");
const {NodeActor} = require("devtools/server/actors/inspector");

const EventsFormActor = ActorClass({
  typeName: "eventsFormActor",

  initialize: function() {
    Actor.prototype.initialize.apply(this, arguments);
  },

  attach: method(function() {
    Events.on(NodeActor, "form", this.onNodeActorForm);
  }, {
    request: {},
    response: {}
  }),

  detach: method(function() {
    Events.off(NodeActor, "form", this.onNodeActorForm);
  }, {
    request: {},
    response: {}
  }),

  onNodeActorForm: function(event) {
    let nodeActor = event.target;
    if (nodeActor.rawNode.id == "container") {
      let form = event.data;
      form.setFormProperty("test-property", "test-value");
    }
  }
});

const EventsFormFront = FrontClass(EventsFormActor, {
  initialize: function(client, form) {
    Front.prototype.initialize.apply(this, arguments);

    this.actorID = form[EventsFormActor.prototype.typeName];
    this.manage(this);
  }
});

exports.EventsFormFront = EventsFormFront;
