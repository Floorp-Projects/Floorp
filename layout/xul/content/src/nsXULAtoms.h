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
#ifndef nsXULAtoms_h___
#define nsXULAtoms_h___

#include "prtypes.h"
#include "nsIAtom.h"

class nsINameSpaceManager;

/**
 * This class wraps up the creation and destruction of the standard
 * set of xul atoms used during normal xul handling. This object
 * is created when the first xul content object is created, and
 * destroyed when the last such content object is destroyed.
 */
class nsXULAtoms {
public:

  static void AddrefAtoms();
  static void ReleaseAtoms();

  // XUL namespace ID, good for the life of the nsXULAtoms object
  static PRInt32  nameSpaceID;

  // Alphabetical list of xul tag and attribute atoms
  static nsIAtom* button;

  static nsIAtom* checkbox;
  static nsIAtom* tristatecheckbox;

  static nsIAtom* radio;

  static nsIAtom* text;
  static nsIAtom* toolbar;
  static nsIAtom* toolbox;
  
  // The tree atoms
  static nsIAtom* tree; // The start of a tree view
  static nsIAtom* treecaption; // The caption of a tree view
  static nsIAtom* treehead; // The header of the tree view
  static nsIAtom* treebody; // The body of the tree view
  static nsIAtom* treeitem; // An item in the tree view
  static nsIAtom* treecell; // A cell in the tree view
  static nsIAtom* treechildren; // The children of an item in the tree viw
  static nsIAtom* treeindentation; // Specifies that the indentation for the level should occur here.
  static nsIAtom* treeallowevents; // Lets events be handled on the cell contents.
  static nsIAtom* treecol; // A column in the tree view
  static nsIAtom* treecolgroup; // A column group in the tree view

  static nsIAtom* progressmeter; 
  static nsIAtom* titledbutton;
  static nsIAtom* mode; 

  static nsIAtom* box; 
  static nsIAtom* flex; 

  static nsIAtom* widget;
  static nsIAtom* window;
};

#endif /* nsXULAtoms_h___ */
