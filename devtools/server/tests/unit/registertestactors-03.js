/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var {method, RetVal, Actor, ActorClass, Front, FrontClass} =
  require("devtools/server/protocol");
var Services = require("Services");

exports.LazyActor = ActorClass({
  typeName: "lazy",

  initialize: function(conn, id) {
    Actor.prototype.initialize.call(this, conn);

    Services.obs.notifyObservers(null, "actor", "instantiated");
  },

  hello: method(function(str) {
    return "world";
  }, {
    response: { str: RetVal("string") }
  })
});

Services.obs.notifyObservers(null, "actor", "loaded");

exports.LazyFront = FrontClass(exports.LazyActor, {
  initialize: function(client, form) {
    Front.prototype.initialize.call(this, client);
    this.actorID = form.lazyActor;

    client.addActorPool(this);
    this.manage(this);
  }
});
