# `SavedFrame`

A `SavedFrame` instance is a singly linked list of stack frames. It represents a
JavaScript call stack at a past moment of execution. Younger frames hold a
reference to the frames that invoked them. The older tails are shared across
many younger frames.


## Accessor Properties of the `SavedFrame.prototype` Object

`source`
:   The source URL for this stack frame, as a string.

`line`
:   The line number for this stack frame.

`column`
:   The column number for this stack frame.

`functionDisplayName`
:   Either SpiderMonkey's inferred name for this stack frame's function, or
    `null`.

`parent`
:   Either this stack frame's parent stack frame (the next older frame), or
    `null` if this is the oldest frame in the captured stack.


## Function Properties of the `SavedFrame.prototype` Object

`toString`
:   Return this frame and its parents formatted as a human readable stack trace
    string.
