# Tab stacks

Dragging a member from the second-row stack bar moves that single tab. Starting
this drag clears native tab multiselection, so reordering, joining another
stack, moving to another window, and detaching all move the same one tab. Drags
started from the native first-row tab strip retain its normal multiselection
behavior.

Proxy drags use Firefox's native tab transfer and detach handlers. The source
window also owns recovery: if a drop removes the proxy before `dragend`, it
waits for the native drag session to finish before releasing the captured drag
state.
