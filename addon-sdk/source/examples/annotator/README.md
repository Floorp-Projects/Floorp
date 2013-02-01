<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

This add-on enables users to add notes, or annotations, to Web pages.

Usage
-----

To switch the annotator on, left-click the pencil icon in the Add-on Bar. The
icon should turn yellow: this indicates that the annotator is active. To switch
it off, click it again. Switching it on/off only stops you from entering
annotations: existing annotations are still displayed.

When the annotator is active and the user moves the mouse over a page element
that can be annotated, the annotator highlights that elements by giving it a
yellow background.

If the user clicks on a highlighted element the add-on opens a dialog for the
user to enter the annotation. When the user hits <return> the annotation is
saved.

Elements which have been annotated are displayed with a yellow border: when the
user moves the mouse over one of these elements, the add-on displays the
annotation associated with that element.

To view all annotations in a list, right-click the pencil icon.

The add-on is deactivated in private browsing mode, meaning that new annotations
can't be created although existing ones are still shown. On exiting private
browsing the add-on returns to its previous activation state.

Known Issues/Limitations
------------------------

It is not possible to delete annotations, or to edit them after creating them,
but it would be simple to add this.

When right-clicking the annotator icon the add-on bar's context-menu is shown:
this is tracked by
[bug 626326](https://bugzilla.mozilla.org/show_bug.cgi?id=626326).

The list of annotations should be anchored to the widget. The annotation
editor, and the annotation itself, should be anchored to the element which is
annotated. The will be done when the implementation of panel-anchoring is
extended.
