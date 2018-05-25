# Actor Best Practices

Some aspects of front and actor design can be tricky to understand, even for experienced engineers.
The following are several best practices you should keep in mind when adding new actors and fronts.

## Actor Should Clean Up Itself, Don't Wait For the Client

In the past, some actors would wait for the client to send a "you are done now" message when the toolbox closes to shutdown the actor.
This seems reasonable at first, but keep in mind that the connection can disappear at any moment.
It may not be possible for the client to send this message.

A better choice is for the actor to do all clean up itself when it's notified that the connection goes away.
Then there's no need for the client to send any clean up message, and we know the actor will be in a good state no matter what.

## Actor Destruction

Ensure that the actor's destroy is really destroying everything that it should. Here's an example from the animation actor:

```js
destroy: function() {
  Actor.prototype.destroy.call(this);
  this.targetActor.off("will-navigate", this.onWillNavigate);
  this.targetActor.off("navigate", this.onNavigate);

  this.stopAnimationPlayerUpdates();
  this.targetActor = this.observer = this.actors = null;
},
```

## Child Actors

With protocol.js actors, if your creates child actors for further functionality, in most cases you should call:

```js
this.manage(child);
```

in the parent after constructing the child, so that the child is destroyed when the parent is.
