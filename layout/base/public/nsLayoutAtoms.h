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
#ifndef nsLayoutAtoms_h___
#define nsLayoutAtoms_h___

#include "nsIAtom.h"

/**
 * This class wraps up the creation (and destruction) of the standard
 * set of atoms used during layout processing. These objects
 * are created when the first presentation context is created and they
 * are destroyed when the last presentation context object is destroyed.
 */

class nsLayoutAtoms {
public:

  static void AddrefAtoms();
  static void ReleaseAtoms();

  // Alphabetical list of media type atoms
  static nsIAtom* all;
  static nsIAtom* aural;
  static nsIAtom* braille;
  static nsIAtom* embossed;
  static nsIAtom* handheld;
  static nsIAtom* print;
  static nsIAtom* projection;
  static nsIAtom* screen;
  static nsIAtom* tty;
  static nsIAtom* tv;

  // Alphabetical list of standard name space prefixes
  static nsIAtom* htmlNameSpace;
  static nsIAtom* xmlNameSpace;
  static nsIAtom* xmlnsNameSpace;

  // Alphabetical list of frame additional child list names
  static nsIAtom* absoluteList;
  static nsIAtom* bulletList;
  static nsIAtom* colGroupList;
  static nsIAtom* fixedList;
  static nsIAtom* floaterList;

  // Alphabetical list of pseudo tag names for non-element content
  static nsIAtom* commentTagName;
  static nsIAtom* textTagName;
  static nsIAtom* processingInstructionTagName;
  static nsIAtom* viewportPseudo;
  static nsIAtom* pagePseudo;

  // Alphabetical list of frame types
  static nsIAtom* areaFrame;
  static nsIAtom* blockFrame;
  static nsIAtom* inlineFrame;
  static nsIAtom* pageFrame;
  static nsIAtom* rootFrame;
  static nsIAtom* scrollFrame;
  static nsIAtom* tableOuterFrame;
  static nsIAtom* tableFrame;
  static nsIAtom* tableRowGroupFrame;
  static nsIAtom* tableRowFrame;
  static nsIAtom* tableCellFrame;
  static nsIAtom* textFrame;
  static nsIAtom* viewportFrame;
};

#endif /* nsLayoutAtoms_h___ */
