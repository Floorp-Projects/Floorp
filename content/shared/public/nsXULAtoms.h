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

  static void AddRefAtoms();
  static void ReleaseAtoms();

  // XUL namespace ID, good for the life of the nsXULAtoms object
  static PRInt32  nameSpaceID;

  /* Declare all atoms

     The atom names and values are stored in nsCSSAtomList.h and
     are brought to you by the magic of C preprocessing

     Add new atoms to nsCSSAtomList and all support logic will be auto-generated
   */
#define XUL_ATOM(_name, _value) static nsIAtom* _name;
#include "nsXULAtomList.h"
#undef XUL_ATOM

};

#endif /* nsXULAtoms_h___ */
