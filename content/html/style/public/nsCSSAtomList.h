/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/******

  This file contains the list of all CSS nsIAtomsand their values
  
  It is designed to be used as inline input to nsCSSAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro CSS_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to CSS_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/


CSS_ATOM(activePseudo, ":active")
CSS_ATOM(afterPseudo, ":after")

CSS_ATOM(beforePseudo, ":before")

CSS_ATOM(buttonLabelPseudo, ":-moz-buttonlabel")

CSS_ATOM(checkedPseudo, ":checked")

CSS_ATOM(disabledPseudo, ":disabled")
CSS_ATOM(dragOverPseudo, ":drag-over")
CSS_ATOM(dragPseudo, ":drag")

CSS_ATOM(enabledPseudo, ":enabled")

CSS_ATOM(firstChildPseudo, ":first-child")
CSS_ATOM(firstNodePseudo, ":first-node")
CSS_ATOM(lastNodePseudo, ":last-node")
CSS_ATOM(focusPseudo, ":focus")

CSS_ATOM(hoverPseudo, ":hover")

CSS_ATOM(langPseudo, ":lang")
CSS_ATOM(linkPseudo, ":link")

CSS_ATOM(menuPseudo, ":menu")

CSS_ATOM(outOfDatePseudo, ":out-of-date")

CSS_ATOM(rootPseudo, ":root")

CSS_ATOM(selectionPseudo, ":selection")

CSS_ATOM(universalSelector, "*")

CSS_ATOM(visitedPseudo, ":visited")
