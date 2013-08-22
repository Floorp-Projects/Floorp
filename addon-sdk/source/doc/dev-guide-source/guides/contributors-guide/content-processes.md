<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

#Content Processes
A content process was supposed to run all the code associated with a single tab.
Conversely, an add-on process was supposed to run all the code associated with a
single add-on. Neither content or add-on proceses were ever actually
implemented, but by the time they were cancelled, the SDK was already designed
with them in mind. To understand this article, it's probably best to read it as
if content and add-on processes actually exist.

To communicate between add-on and content processes, the SDK uses something
called content scripts. These are explained in the first section. Content
scripts communicate with add-on code using something called event emitters.
These are explained in the next section. Content workers combine these ideas,
allowing you to inject a content script into a content process, and
automatically set up a communication channel between them. These are explained
in the third section.

In the next section, we will look at how content scripts interact with the DOM
in a content process. There are several caveats here, all of them related to
security, that might cause things to not behave in the way you might expect.

The final section explains why the SDK still uses the notion of content scripts
and message passing, even though the multiprocess model for which they were
designed never materialized. This, too, is primarily related to security.

##Content Scripts
When the SDK was first designed, Firefox was being refactored towards a
multiprocess model. In this model, the UI would be rendered in one process
(called the chrome process), whereas each tab and each add-on would run in their
own dedicated process (called content and add-on processes, respectively). The
project behind this refactor was known as Electrolysis, or E10s. Although E10s
has now been suspended, the SDK was designed with this multiprocess model in
mind. Afterwards, it was decided to keep the design the way it is: even though
its no longer necessary, it turns out that from a security point of view there
are several important advantages to thinking about content and add-on code as
living in different processes.

Many add-ons have to interact with content. The problem with the multiprocess
model is that add-ons and content are now in different processes, and scripts in
one process cannot interact directly with scripts in another. We can, however,
pass JSON messages between scripts in different processes. The solution we've
come up with is to introduce the notion of content scripts. A content script is
a script that is injected into a content process by the main script running in
the add-on process. Content scripts differ from scripts that are loaded by the
page itself in that they are provided with a messaging API that can be used to
send messages back to the add-on script.

##Event Emitters
The messaging API we use to send JSON messages between scripts in different
processes is based on the use of event emitters. An event emitter maintains a
list of callbacks (or listeners) for one or more named events. Each event
emitter has several methods: the method on is used to add a listener for an
event. Conversely, the method removeListener is used to remove a listener for an
event. The method once is a helper function which adds a listener for an event,
and automatically removes it the first time it is called.

Each event emitter has two associated emit functions. One emit function is
associated with the event emitter itself. When this function is called with a
given event name, it calls all the listeners currently associated with that
event. The other emit function is associated with another event emitter: it was
passed as an argument to the constructor of this event emitter, and made into a
method. Calling this method causes an event to be emitted on the other event
emitter.

Suppose we have two event emitters in different processes, and we want them to
be able to emit events to each other. In this case, we would replace the emit
function passed to the constructor of each emitter with a function that sends a
message to the other process. We can then hook up a listener to be called when
this message arrives at the other process, which in turn calls the emit function
on the other event emitter. The combination of this function and the
corresponding listener is referred to as a pipe. 

##Content Workers
A content worker is an object that is used to inject content scripts into a
content process, and to provide a pipe between each content script and the main
add-on script. The idea is to use a single content worker for each content
process. The constructor for the content worker takes an object containing one
or more named options. Among other things, this allows us to specify one or more
content scripts to be loaded.

When a content script is first loaded, the content worker automatically imports
a messaging API that allows the it to emit messages over a pipe. On the add-on
side, this pipe is exposed via the the port property on the worker. In addition
to the port property, workers also support the web worker API, which allows
scripts to send messages to each other using the postMessage function. This
function uses the same pipe internally, and causes a 'message' event to be
emitted on the other side.

As explained earlier, Firefox doesn't yet use separate processes for tabs or
add-ons, so instead, each content script is loaded in a sandbox. Sandboxes were
explained [this article]("dev-guide/guides/contributors-guide/modules.html").

##Accessing the DOM
The global for the content sandbox has the window object as its prototype. This
allows the content script to access any property on the window object, even
though that object lives outside the sandbox. Recall that the window object
inside the sandbox is actually a wrapper to the real object. A potential
problem with the content script having access to the window object is that a
malicious page could override methods on the window object that it knows are
being used by the add-on, in order to trick the add-on into doing something it
does not expect. Similarly, if the content script defines any values on the
window object, a malicious page could potentially steal that information.

To avoid problems like this, content scripts should always see the built-in
properties of the window object, even when they are overridden by another
script. Conversely, other scripts should not see any properties added to the
window object by the content script. This is where xray wrappers come in. Xray
wrappers automatically wrap native objects like the window object, and only
exposes their native properties, even if they have been overridden on the
wrapped object. Conversely, any properties defined on the wrapper are not
visible from the wrapped object. This avoids both problems we mentioned earlier.

The fact that you can't override the properties of the window object via a
content script is sometimes inconvenient, so it is possible to circumvent this:
by defining the property on window.wrappedObject, the property is defined on the
underlying object, rather than the wrapper itself. This feature should only be
used when you really need it, however.

##A few Notes on Security
As we stated earlier, the SDK was designed with multiprocess support in mind,
despite the fact that work on implementing this in Firefox has currently been
suspended. Since both add-on modules and content scripts are currently loaded in
sandboxes rather than separate processes, and sandboxes can communicate with
each other directly (using imports/exports), you might be wondering why we have
to go through all the trouble of passing messages between add-on and content
scripts. The reason for this extra complexity is that the code for add-on
modules and content scripts has different privileges. Every add-on module can
get chrome privileges simply by asking for them, whereas content scripts have
the same privileges as the page it is running on.

When two sandboxes have the same privileges, a wrapper in one sandbox provides
transparent access to an object in the other sandbox. When the two sandboxes
have different privileges, things become more complicated, however. Code with
content privileges should not be able to acces code with chrome privileges, so
we use specialized wrappers, called security wrappers, to limit access to the
object in the other sandbox. The xray wrappers we saw earlier are an example of
such a security wrapper. Security wrappers are created automatically, by the
underlying host application.

A full discussion of the different kinds of security wrappers and how they work
is out of scope for this document, but the main point is this: security wrappers
are very complex, and very error-prone. They are subject to change, in order to
fix some security leak that recently popped up. As a result, code that worked
just fine last week suddenly does not work the way you expect. By only passing
messages between add-on modules and content scripts, these problems can be
avoided, making your add-on both easier to debug and to maintain.
