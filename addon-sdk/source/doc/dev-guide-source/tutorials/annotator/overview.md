<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Annotator Design Overview #

The annotator uses content scripts to build user interfaces, get user input,
and examine the DOM of pages loaded by the user.

Meanwhile the `main` module contains the application logic and mediates
interactions between the different SDK objects.

We could represent the basic interactions between the `main` module and the
various content scripts like this:

<img class="image-center"
src="static-files/media/annotator/annotator-design.png" alt="Annotator Design">

## User Interface ##

The annotator's main user interface consists of a widget and three panels.

* The widget is used to switch the annotator on and off, and to display a list
of all the stored annotations.
* The **annotation-editor** panel enables the user to enter a new annotation.
* The **annotation-list** panel shows a list of all stored annotations.
* The **annotation** panel displays a single annotation.

Additionally, we use the `notifications` module to notify the user when the
add-on's storage quota is full.

## Working with the DOM ##

We'll use two page-mods to interact with the DOMs of pages that the user has
opened.

* The **selector** enables the user to choose an element to annotate.
It identifies page elements which are eligible for annotation, highlights them
on mouseover, and tells the main add-on code when the user clicks a highlighted
element.

* The **matcher** is responsible for finding annotated elements: it is
initialized with the list of annotations and searches web pages for the
elements they are associated with. It highlights any annotated elements that
are found. When the user moves the mouse over an annotated element
the matcher tells the main add-on code, which displays the annotation panel.

## Working with Data ##

We'll use the `simple-storage` module to store annotations.

Because we are recording potentially sensitive information, we want to prevent
the user creating annotations when in private browsing mode. The simplest way
to do this is to omit the
[`"private-browsing"` key](dev-guide/package-spec.html#permissions) from the
add-on's "package.json" file. If we do this, then the add-on won't see any
private windows, and the annotator's widget will not appear in any private
windows.

## Getting Started ##


Let's get started by creating a directory called "annotator". Navigate to it
and type `cfx init`.

Next, we will implement the
[widget](dev-guide/tutorials/annotator/widget.html).
