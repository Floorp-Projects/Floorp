<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Many objects in the SDK can broadcast events. For example, a
[`panel`](modules/sdk/panel.html) instance emits an `show` event when the
panel is shown.

The `event/target` module enables you to create objects that broadcast
events. Users of the object can listen to the events using the standard
`on()` and `once()` functions.

Also see the
[tutorial on implementing event targets](dev-guide/tutorials/event-targets.html)
to get started with this API.

This module provides an exemplar `EventTarget` object, that implements
an interface for adding and removing event listeners of a specific type.
`EventTarget` is the base class for all objects in SDK on which events
are emitted.

## Instantiation

It's easy to create event target objects, no special arguments are required.

    const { EventTarget } = require("sdk/event/target");
    let target = EventTarget();

For a convenience though optional `options` arguments may be used, in which
case all the function properties with keys like: `onMessage`, `onMyEvent`...
will be auto registered for associated `'message'`, `'myEvent'` events on
the created instance. _All other properties of `options` will be ignored_.

## Adding listeners

`EventTarget` interface defines `on` method, that can be used to register
event listeners on them for the given event type:

    target.on('message', function onMessage(message) {
      // Note: `this` pseudo variable is an event `target` unless
      // intentionally overridden via `.bind()`.
      console.log(message);
    });

Sometimes event listener may care only about very first event of specific
`type`. `EventTarget` interface defines convenience method for adding one
shot event listeners via method `once`. Such listeners are called only once
next time event of the specified type is emitted:

    target.once('ready', function onReady() {
      // Do the thing once ready!
    });

## Removing listeners

`EventTarget` interface defines API for unregistering event listeners, via
`removeListener` method:

    target.removeListener('message', onMessage);

## Emitting events

`EventTarget` interface intentionally does not defines an API for emitting
events. In majority of cases party emitting events is different from party
registering listeners. In order to emit events one needs to use `event/core`
module instead:

    let { emit } = require('sdk/event/core');

    target.on('hi', function(person) { console.log(person + ' says hi'); });
    emit(target, 'hi', 'Mark'); // info: 'Mark says hi'

For more details see **event/core** documentation.

## More details

Listeners registered during the event propagation (by one of the listeners)
won't be triggered until next emit of the matching type:

    let { emit } = require('sdk/event/core');

    target.on('message', function onMessage(message) {
      console.log('listener triggered');
      target.on('message', function() {
        console.log('nested listener triggered');
      });
    });

    emit(target, 'message'); // info: 'listener triggered'
    emit(target, 'message'); // info: 'listener triggered'
                             // info: 'nested listener triggered'

Exceptions in the listeners can be handled via `'error'` event listeners:

    target.on('boom', function() {
      throw Error('Boom!');
    });
    target.once('error', function(error) {
      console.log('caught an error: ' + error.message);
    });
    emit(target, 'boom');
    // info: caught an error: Boom!

If there is no listener registered for `error` event or if it also throws
exception then such exceptions are logged into a console.

## Chaining

Emitters can also have their methods chained:

    target.on('message', handleMessage)
      .on('data', parseData)
      .on('error', handleError);

<api name="EventTarget">
@class
`EventTarget` is an exemplar for creating an objects that can be used to
add / remove event listeners on them. Events on these objects may be emitted
via `emit` function exported by [`event/core`](modules/sdk/event/core.html)
module.

<api name="initialize">
@method
Method initializes `this` event source. It goes through properties of a
given `options` and registers listeners for the ones that look like
event listeners.
</api>

<api name="on">
@method
Registers an event `listener` that is called every time events of
specified `type` are emitted.

    worker.on('message', function (data) {
      console.log('data received: ' + data)
    });

@param type {String}
   The type of event.
@param listener {Function}
   The listener function that processes the event.
@returns {EventTarget}
   Returns the EventTarget instance
</api>

<api name="once">
@method
Registers an event `listener` that is called only once:
the next time an event of the specified `type` is emitted.
@param type {String}
   The type of event.
@param listener {Function}
   The listener function that processes the event.
@returns {EventTarget}
   Returns the EventTarget instance
</api>

<api name="removeListener">
@method
Removes an event `listener` for the given event `type`.
@param type {String}
   The type of event.
@param listener {Function}
   The listener function that processes the event.
@returns {EventTarget}
   Returns the EventTarget instance
</api>

<api name="off">
@method
An alias for [removeListener](modules/sdk/event/target.html#removeListener(type, listener)).
</api>

</api>
