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
#ifndef nsHTMLAtoms_h___
#define nsHTMLAtoms_h___

#include "nsIAtom.h"

#define NS_HTML_BASE_HREF   "_base_href"
#define NS_HTML_BASE_TARGET "_base_target"

/**
 * This class wraps up the creation (and destruction) of the standard
 * set of html atoms used during normal html handling. This objects
 * are created when the first html content object is created and they
 * are destroyed when the last html content object is destroyed.
 */
class nsHTMLAtoms {
public:

  static void AddRefAtoms();
  static void ReleaseAtoms();

  /* Declare all atoms

     The atom names and values are stored in nsHTMLAtomList.h and
     are brought to you by the magic of C preprocessing

     Add new atoms to nsHTMLAtomList and all support logic will be auto-generated
   */
#define HTML_ATOM(_name, _value) static nsIAtom* _name;
#include "nsHTMLAtomList.h"
#undef HTML_ATOM
};

#endif /* nsHTMLAtoms_h___ */
