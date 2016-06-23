/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var {method, RetVal, Actor, ActorClassWithSpec, Front, FrontClassWithSpec,
     generateActorSpec} = require("devtools/shared/protocol");
var Services = require("Services");

const lazySpec = generateActorSpec({
  typeName: "lazy",

  methods: {
    hello: {
      response: { str: RetVal("string") }
    }
  }
});

exports.LazyActor = ActorClassWithSpec(lazySpec, {
  initialize: function (conn, id) {
    Actor.prototype.initialize.call(this, conn);

    Services.obs.notifyObservers(null, "actor", "instantiated");
  },

  hello: function (str) {
    return "world";
  }
});

Services.obs.notifyObservers(null, "actor", "loaded");

exports.LazyFront = FrontClassWithSpec(lazySpec, {
  initialize: function (client, form) {
    Front.prototype.initialize.call(this, client);
    this.actorID = form.lazyActor;

    client.addActorPool(this);
    this.manage(this);
  }
});
