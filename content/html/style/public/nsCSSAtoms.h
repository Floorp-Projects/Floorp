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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsCSSAtoms_h___
#define nsCSSAtoms_h___

#include "nsIAtom.h"

/**
 * This class wraps up the creation (and destruction) of the standard
 * set of CSS atoms used during normal CSS handling. These objects
 * are created when the first CSS style sheet is created and they
 * are destroyed when the last CSS style sheet is destroyed.
 */
class nsCSSAtoms {
public:

  static void AddrefAtoms();
  static void ReleaseAtoms();

  // Alphabetical list of css atoms
  static nsIAtom* activePseudo;
  static nsIAtom* afterPseudo;
  
  static nsIAtom* beforePseudo;

  static nsIAtom* disabledPseudo;

  static nsIAtom* enabledPseudo;

  static nsIAtom* firstChildPseudo;
  static nsIAtom* focusPseudo;

  static nsIAtom* hoverPseudo;

  static nsIAtom* langPseudo;
  static nsIAtom* linkPseudo;

  static nsIAtom* outOfDatePseudo;  // Netscape extension

  static nsIAtom* selectedPseudo;
  static nsIAtom* selectionPseudo;

  static nsIAtom* universalSelector;

  static nsIAtom* visitedPseudo;
};

#endif /* nsCSSAtoms_h___ */
