############################
Editor module specific rules
############################

The editor module has not been maintained aggressively about a decade. Therefore, this module needs
to be treated as a young module or in a transition period to align the behavior to the other
browsers and take modern C++ style.

Undoubtedly, this editor module is under rewritten for modern and optimized for current specs.
Additionally, this module does really complicated things which may cause security issues.
Therefore, there are specific rules:

Treat other browsers behavior as standard if the behavior is reasonable
=======================================================================

The editing behavior is not standardized since as you see too many lines in the editor classes, the
number of cases which need to handle edge cases is crazy and that makes it impossible to standardize.
Additionally, our editor behavior is not so stable. Some behaviors were aligned to Internet Explorer,
some other behaviors were not for making "better" UX for users of email composer and HTML composer
which were in SeaMonkey, and the other browser engines (Blink and WebKit) have same roots but the
behavior is different from both IE and Gecko.

Therefore, there were no reference behavior.

In these days, compatibility between browsers becomes more important, and fortunately, the behavior
of Blink (Chrome/Chromium) which has the biggest market share is more reasonable than ours in a lot
of cases. Therefore, if we get web-compat issue reports, we should align the behavior to Blink in
theory.

However, if Blink's behavior is also odd, this is the worst case. In this case, we should try to
align the behavior to WebKit if and only if WebKit's behavior is different from Blink and
reasonable, or doing something "better" for hiding the issue from web-apps and file an issue to the
Editing Working Group with creating a "tentative" web-platform test.

Don't make methods of editor classes public if they are used only by helper classes
===================================================================================

Although this is a silly rule. Of course, APIs of editor classes need to be public for the other
modules. However, the other methods which are used only by helper classes in the editor module --the
methods may be crashed if called by the other modules because editor classes store and guarantee the
colleagues (e.g., ``Selection``) when it starts to handle an edit action (edit command or
operation)--  does not want to do it for the performance reason. Therefore, such methods are now
declared as protected methods and the caller classes are registered as friends.

For solving this issue, we could split the editor classes one is exported and the other is not
exposed, and make the former to proxies and own the latter.  However, this approach might cause
performance regressions and requires a lot of line changes (at least each method definition and
warning messages at the caller sides).  Tracked in
`bug 1555916 <https://bugzilla.mozilla.org/show_bug.cgi?id=1555916>`__.

Steps to handle one editor command or operation
===============================================

One edit command or operation is called "edit action" in the editor module.  Handling it starts
when an XPCOM method or a public method which is named as ``*AsAction``. Those methods create
``AutoEditActionDataSetter`` in the stack first, then, call one of ``CanHandle()``,
``CanHandleAndMaybeDispatchBeforeInputEvent()`` or ``CanHandleAndFlushPendingNotifications()``.
If ``CanHandleAndMaybeDispatchBeforeInputEvent()`` causes dispatching ``beforeinput`` event and if
the event is consumed by the web app, it returns ``NS_ERROR_EDITOR_ACTION_CANCELED``. In this case,
the method can do anything due to the ``beforeinput`` event definition.

At this time, ``AutoEditActionDataSetter`` stores ``Selection`` etc which are required for handling
the edit action in it and set ``EditorBase::mEditActionData`` to its address. Then all methods of
editor can access the objects via the pointer (typically wrapped in inline methods) and the lifetime
of the objects are guaranteed.

Then, the methods call one or more edit-sub action handlers.  E.g., when user types a character
with a non-collapsed selection range, editor needs to delete the selected content first and insert
the character there. For implementing this behavior, "insert text" edit action handler needs to call
"delete selection" sub-action handler and "insert text" sub-action handler. The sub-edit action
handlers are named as ``*AsSubAction``.

The callers of edit sub-action handlers or the handlers themselves create ``AutoPlaceholderBatch``
in the stack. This creates a placeholder transaction to make all transactions undoable with one
"undo" command.

Then, each edit sub-action handler creates ``AutoEditSubActionNotifier`` in the stack and if it's
the topmost edit sub-action handling, ``OnStartToHandleTopLevelEditSubAction()`` is called at the
creation and ``OnEndHandlingTopLevelEditSubAction()`` is called at the destruction. The latter will
clean up the modified range, e.g., remove unnecessary empty nodes.

Finally, the edit sub-actions does something while ``AutoEditSubActionNotifier`` is alive. Helper
methods of edit sub-action handlers are typically named as ``*WithTransaction`` because they are
done with transaction classes for making everything undoable.

Don't update Selection immediately
==================================

Changing the ranges of ``Selection`` is expensive (due ot validating new range, notifying new
selected or unselected frames, notifying selection listeners, etc), and retrieving current
``Selection`` ranges at staring to handle something makes the code statefull which is harder to
debug when you investigate a bug. Therefore, each method should return new caret position or
update ranges given as in/out parameter of ``AutoRangeArray``.  ``Result<CaretPoint, nsresult>``
is a good result type for the former, and the latter is useful style if the method needs to keep
``Selection`` similar to given ranges, e.g., when paragraphs around selection are changed to
different type of blocks. Finally, edit sub-action handler methods should update ``Selection``
before destroying ``AutoEditSubActionNotifier`` whose post-processing requires ``Selection``.

Don't add new things into OnEndHandlingTopLevelEditSubAction()
==============================================================

When the topmost edit sub-action is handled, ``OnEndHandlingTopLevelEditSubAction`` is called and
it cleans up something in (or around) the modified range. However, this "post-processing" approach
makes it harder to change the behavior for fixing web-compat issues. For example, it deletes empty
nodes in the range, but if only some empty nodes are inserted intentionally, it doesn't have the
details and may unexpectedly delete necessary empty nodes.

Instead, new things should be done immediately at or after modifying the DOM tree, and if it
requires to disable the post-processing, add new ``bool`` flag to
``EditorBase::TopLevelEditSubActionData`` and when it's set, make
``OnEndHandlingTopLevelEditSubAction`` stop doing something.

Don't use NS_WARN_IF for checking NS_FAILED, isErr() and Failed()
=================================================================

The warning messages like ``NS_FAILED(rv)`` does not help except the line number, and in the cases
of that we get web-compat reports, somewhere in the editor modules may get unexpected result. For
saving the investigation time of web-compat issues, each failure should warn which method call
failed, for example:

.. code:: cpp

  nsresult rv = DoSomething();
  if (NS_FAILED(rv)) {
    NS_WARNING("HTMLEditor::DoSomething() failed");
    return rv;
  }

These warnings will let you know the stack of failure in debug build. In other words, when you
investigate a web-compat issue in editor, you should do the steps to reproduce in debug build first.
Then, you'd see failure point stack in the terminal.

Return NS_ERROR_EDITOR_DESTROYED when editor gets destroyed
===========================================================

The most critical error while an editor class method is running is what the editor instance is
destroyed by the web app. This can be checked with a call of ``EditorBase::Destroyed()`` and
if it returns ``true``, methods should return ``NS_ERROR_EDITOR_DESTROYED`` with stopping handling
anything. Then, all callers which handle the error result properly will stop handling too.
Finally, public methods should return ``EditorBase::ToGenericNSResult(rv)`` instead of exposing
an internal error of the editor module.

Note that destroying the editor is intentional thing for the web app. Thus we should not throw
exception for this failure reason. Therefore, the public methods shouldn't return error.

When you make a method return ``NS_ERROR_EDITOR_DESTROYED`` properly, you should mark the method
as ``[[nodiscard]]``. In other words, if you see ``[[nodiscard]]`` in method definition and it
returns ``nsresult`` or ``Result<*, nsresult>``, the method callers do not need to check
``Destroyed()`` by themselves.

Use reference instead of pointer as far as possible
===================================================

When you create or redesign a method, it should take references instead of pointers if they take.
This rule forces that the caller to do null-check and this avoids a maybe unexpected case like:

.. code:: cpp

  inline bool IsBRElement(const nsINode* aNode) {
    return aNode && aNode->IsHTMLElement(nsGkAtoms::br);
  }

  void DoSomethingExceptIfBRElement(const nsINode* aNode) {
    if (IsBRElement(aNode)) {
      return;
    }
    // Do something for non-BR element node.
  }

In this case, ``DoSomethingExceptIfBRElement`` expects that ``aNode`` is never ``nullptr`` but it
could be at least in build time. Using reference fixes this mistake at build time.

Use ``EditorUtils`` or ``HTMLEditUtils`` for stateless methods
==============================================================

When you create a new static method to the editor classes or a new inline method in cpp file which
defines the editor classes, please check if it's a common method which may be used from other
places in the editor module.  If it's possible to be used only in ``HTMLEditor`` or its helper
classes, the method should be in ``HTMLEditUtils``.  If it's possible be used in ``EditorBase`` or
``TextEditor`` or their helper classes, it should be in ``EditorUtils``.

Don't use bool argument
=======================

If you create a new method which take one or more ``bool`` arguments, use ``enum class`` instead
since ``true`` or ``false`` in the caller side is not easy to read. For example, you must not
be able to understand what this example mean:

.. code:: cpp

  if (IsEmpty(aNode, true)) {

For avoiding this issue, you should create new ``enum class`` for each.  E.g.,

.. code:: cpp

  if (IsEmpty(aNode, TreatSingleBR::AsVisible)) {

Basically, both ``enum class`` name and its value names explains what it means fluently. However, if
it's impossible, use ``No`` and ``Yes`` for the value like:

.. code:: cpp

  if (DoSomething(aNode, OnlyIfEmpty::Yes)) {

Don't use out parameters
========================

In most cases, editor methods meet error of low level APIs, thus editor methods usually return error
code. On the other hand, a lot of code need to return computed things, e.g., new caret position,
whether it's handled, ignored or canceled, a target node looked for, etc. We used ``nsresult`` for
the return value type and out parameters for the other results, but it makes callers scattering a
lot of auto variables and reusing them makes the code harder to understand.

Now we can use ``mozilla::Result<Foo, nsresult>`` instead.
