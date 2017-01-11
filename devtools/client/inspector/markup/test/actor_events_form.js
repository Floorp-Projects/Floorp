/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test actor is used for testing the addition of custom form data
// on NodeActor. Custom form property is set when 'form' event is sent
// by NodeActor actor (see 'onNodeActorForm' method).

const Events = require("sdk/event/core");
const {ActorClassWithSpec, Actor, FrontClassWithSpec, Front, generateActorSpec} =
  require("devtools/shared/protocol");

const {NodeActor} = require("devtools/server/actors/inspector");

var eventsSpec = generateActorSpec({
  typeName: "eventsFormActor",

  methods: {
    attach: {
      request: {},
      response: {}
    },
    detach: {
      request: {},
      response: {}
    }
  }
});

var EventsFormActor = ActorClassWithSpec(eventsSpec, {
  initialize() {
    Actor.prototype.initialize.apply(this, arguments);
  },

  attach() {
    Events.on(NodeActor, "form", this.onNodeActorForm);
  },

  detach() {
    Events.off(NodeActor, "form", this.onNodeActorForm);
  },

  onNodeActorForm(event) {
    let nodeActor = event.target;
    if (nodeActor.rawNode.id == "container") {
      let form = event.data;
      form.setFormProperty("test-property", "test-value");
    }
  }
});

var EventsFormFront = FrontClassWithSpec(eventsSpec, {
  initialize(client, form) {
    Front.prototype.initialize.apply(this, arguments);

    this.actorID = form[EventsFormActor.prototype.typeName];
    this.manage(this);
  }
});

exports.EventsFormFront = EventsFormFront;
