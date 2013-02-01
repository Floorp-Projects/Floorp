<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Annotator: a More Complex Add-on #

In this tutorial we'll build an add-on that uses many of the SDK's
[high-level APIs](modules/high-level-modules.html).

The add-on is an annotator: it enables the user to select elements of web pages
and enter notes (annotations) associated with them. The annotator stores the
notes. Whenever the user loads a page containing annotated elements these
elements are highlighted, and if the user moves the mouse over an annotated
element its annotation is displayed.

Next we'll give a quick overview of the annotator's design, then go through
the implementation, step by step.

If you want to refer to the complete add-on you can find it under the
`examples` directory.

* [Design Overview](dev-guide/tutorials/annotator/overview.html)

* [Implementing the Widget](dev-guide/tutorials/annotator/widget.html)

* [Creating Annotations](dev-guide/tutorials/annotator/creating.html)

* [Storing Annotations](dev-guide/tutorials/annotator/storing.html)

* [Displaying Annotations](dev-guide/tutorials/annotator/displaying.html)

