/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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

CSS_ATOM(checkedPseudo, ":checked")

CSS_ATOM(disabledPseudo, ":disabled")
CSS_ATOM(dragOverPseudo, ":drag-over")
CSS_ATOM(dragPseudo, ":drag")

CSS_ATOM(enabledPseudo, ":enabled")

CSS_ATOM(firstChildPseudo, ":first-child")
CSS_ATOM(focusPseudo, ":focus")

CSS_ATOM(hoverPseudo, ":hover")

CSS_ATOM(langPseudo, ":lang")
CSS_ATOM(linkPseudo, ":link")

CSS_ATOM(menuPseudo, ":menu")
CSS_ATOM(mozDummyOptionPseudo, ":-moz-dummy-option")

CSS_ATOM(outOfDatePseudo, ":out-of-date")

CSS_ATOM(selectionPseudo, ":selection")

CSS_ATOM(universalSelector, "*")

CSS_ATOM(visitedPseudo, ":visited")
