#######################
Editor module structure
#######################

This document explains the structure of the editor module and overview of classes.

Introduction
============

This module implements the builtin editors of editable elements or documents, and this does **not**
implement the interface with DOM API and visual feedback of the editing UI. In other words, this
module implements DOM tree editors.

Directories
===========

composer
--------

Previously, this directory contained "Composer" UI related code.  However, currently, this
directory contains ``nsEditingSession`` and ``ComposerCommandsUpdater``.

libeditor
---------

This is the main directory which contains "core" implementation of editors.

spellchecker
------------

Despite of the directory name, implementation of the spellchecker is **not** here. This directory
contains only a bridge between editor classes and the spellchecker and serialized text of editable
content for spellchecking.

txmgr
-----

This directory contains transaction items and transaction classes. They were designed for generic
use cases, e.g., managing undo/redo of bookmarks/history of browser, etc, but they are used only by
the editor.

Main classes
============

EditorBase
----------

``EditorBase`` class is an abstract class of editors.  This inherits ``nsIEditor`` XPCOM interface,
implement common features which work with instance of classes, and exposed by
``mozilla/EditorBase.h``.

TextEditor
----------

``TextEditor`` class is the implementation of plaintext editor which works with ``<input>`` and
``<textarea>``.  Its exposed root is the host HTML elements, however, the editable root is an
anonymous ``<div>`` created in a native anonymous subtree under the exposed root elements. This
creates a ``Text`` node as the first child of the anonymous ``<div>`` and modify its data. If the text
data ends with a line-break, i.e., the last line is empty, append a ``<br>`` element for making the
empty last line visible.

This also implements password editor. It works almost same as normal text editor, but each character
may be masked by masked character such as "●" or "*" by the layout module for the privacy.
Therefore, this manages masked/unmasked range of password and maybe making typed character
automatically after a while for mobile devices.

This is exposed with ``mozilla/TextEditor.h``.

Selection in TextEditor
^^^^^^^^^^^^^^^^^^^^^^^

Independent ``Selection`` and ``nsFrameSelection`` per ``<input>`` or ``<textarea>``.

Lifetime of TextEditor
^^^^^^^^^^^^^^^^^^^^^^

Created when an editable ``<textarea>`` is created or a text-editable ``<input>`` element gets focus.
Note that the initialization may run asynchronously if it's requested when it's not safe to run
script. Destroyed when the element becomes invisible. Note that ``TextEditor`` is recreated when
every reframe of the host element.  This means that when the size of ``<input>`` or ``<textarea>``
is changed for example, ``TextEditor`` is recreated and forget undo/redo transactions, but takes
over the value, selection ranges and composition of IME from the previous instance.

HTMLEditor
----------

``HTMLEditor`` class is the implementation of rich text editor which works with ``contenteditable``,
``Document.designMode`` and XUL ``<editor>``. Its instance is created per document even if the
document has multiple elements having ``contenteditable`` attribute. Therefore, undo/redo
transactions are shared in all editable regions.

This is exposed with ``mozilla/HTMLEditor.h``.

Selection in HTMLEditor
^^^^^^^^^^^^^^^^^^^^^^^

The instance for the ``Document`` and ``Window``. When an editable element gets focus, ``HTMLEditor``
sets the ancestor limit of ``Selection`` to the focused element or the ``<body>`` of the ``Document``.
Then, ``Selection`` cannot cross boundary of the limiter element.

Lifetime of HTMLEditor
^^^^^^^^^^^^^^^^^^^^^^

Created when first editable region is created in the ``Document``.  Destroyed when last editable
region becomes non-editable.

Currently, even while ``HTMLEditor`` is handling an edit command/operation (called edit action in
editor classes), each DOM mutation can be tracked with legacy DOM mutation events synchronously.
Thus, after changing the DOM tree from ``HTMLEditor``, any state could occur, e.g., the editor
itself may have been destroyed, the DOM tree have been modified, the ``Selection`` have been
modified, etc. This issue is tracked in
`bug 1710784 <https://bugzilla.mozilla.org/show_bug.cgi?id=1710784>`__.


EditorUtils
-----------

This class has only static utility methods which are used by ``EditorBase`` or ``TextEditor`` and
may be used by ``HTMLEditor`` too.  I.e., the utility methods which are used **not** only by
``HTMLEditor`` should be implemented in this class.

Typically, sateless methods should be implemented as ``static`` methods of utility classes because
editor classes have too many methods and fields.

This class is not exposed.

HTMLEditUtils
-------------

This class has only static utility methods which are used only by ``HTMLEditor``.

This class is not exposed.

AutoRangeArray
--------------

This class is a stack only class and intended to copy of normal selection ranges.  In the new code,
`Selection` shouldn't be referred directly, instead, methods should take reference to this instance
and modify it.  Finally, root caller should apply the ranges to `Selection`.  Then, `HTMLEditor`
does not need to take care of unexpected `Selection` updates by legacy DOM mutation event listeners.

This class is not exposed.

EditorDOMPoint, EditorRawDOMPoint, EditorDOMPointInText, EditorRawDOMPointInText
--------------------------------------------------------------------------------

It represents a point in a DOM tree with one of the following:

* Container node and offset in it
* Container node and child node in it
* Container node and both offset and child node in it

In most cases, instances are initialized with a container and only offset or child node.  Then,
when ``Offset()`` or ``GetChild()`` is called, the last one is "fixed".  After inserting new child
node before the offset and/or the child node, ``IsSetAndValid()`` will return ``false`` since the
child node is not the child at the offset.

If you want to keep using after modifying the DOM tree, you can make the instance forget offset or
child node with ``AutoEditorDOMPointChildInvalidator`` and ``AutoEditorDOMRangeChildrenInvalidator``.
The reason why the forgetting methods are not simply exposed is, ``Offset()`` and ``GetChild()``
are available even after the DOM tree is modified to get the cached offset and child node,
additionally, which method may modify the DOM tree may be not clear for developers.  Therefore,
creating a block only for these helper classes makes the updating point clearer.

These classes are exposed with ``mozilla/EditorDOMPoint.h``.

EditorDOMRange, EditorRawDOMRange, EditorDOMRangeInTexts, EditorRawDOMRangeInTexts
----------------------------------------------------------------------------------

It represents 2 points in a DOM tree with 2 ``Editor*DOMPoint(InText)``.  Different from ``nsRange``,
the instances do not track the DOM tree changes. Therefore, the initialization is much faster than
``nsRange`` and can be in the stack.

These classes are exposed with ``mozilla/EditorDOMPoint.h``.

AutoTrackDOMPoint, AutoTrackDOMRange
------------------------------------

These methods updates ``Editor*DOMPoint(InText)`` or ``Editor*DOMRange(InTexts)`` at destruction
with applying the changes caused by the editor instance.  In other words, they don't track the DOM
tree changes by the web apps like changes from legacy DOM mutation event listeners.

These classes are currently exposed with ``mozilla/SelectionState.h``, but we should stop exposing
them.

WSRunScanner
------------

A helper class of ``HTMLEditor``. This class scans previous or (inclusive) next visible thing from
a DOM point or a DOM node. This is typically useful for considering whether a `<br>` is visible or
invisible due to near a block element boundary, finding nearest editable character from caret
position, etc.  However, the running cost is **not** cheap, thus if you find another way to consider
it simpler, use it instead, and also this does not check the actual style of the nodes (visible vs.
invisible, block vs. inline), thus you'd get unexpected result in tricky cases.

This class is not exposed.

WhiteSpaceVisibilityKeeper
--------------------------

A helper class of ``HTMLEditor`` to handle collapsible white-spaces as what user expected. This
class currently handles white-space normalization (e.g., when user inputs multiple collapsible
white-spaces, this replaces some of them to NBSPs), but the behavior is different from the other
browsers. We should re-implement this with emulating the other browsers' behavior as far as possible,
but currently it's put off due to not affecting UX (tracked in
`bug 1658699 <https://bugzilla.mozilla.org/show_bug.cgi?id=1658699>`__.

This class is not exposed.

\*Transaction
-------------

``*Transaction`` classes represents a small transaction of updating the DOM tree and implements
"do", "undo" and "redo" of the update.

Note that each class instance is created too many (one edit action may cause multiple transactions).
Therefore, each instance must be smaller as far as possible, and if you have an idea to collapse
multiple instances to one instance, you should fix it. Then, users can run Firefox with smaller
memory devices especially if the transaction is used in ``TextEditor``.
