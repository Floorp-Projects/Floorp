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

#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"

static const char kXULNameSpace[] = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

PRInt32  nsXULAtoms::nameSpaceID;

// define storage for all atoms
#define XUL_ATOM(_name, _value) nsIAtom* nsXULAtoms::_name;
#include "nsXULAtomList.h"
#undef XUL_ATOM


static nsrefcnt gRefCnt;
static nsINameSpaceManager* gNameSpaceManager;

void nsXULAtoms::AddRefAtoms() {

  if (gRefCnt == 0) {
    /* XUL Atoms registers the XUL name space ID because it's a convenient
       place to do this, if you don't want a permanent, "well-known" ID.
    */
    if (NS_SUCCEEDED(NS_NewNameSpaceManager(&gNameSpaceManager))) {
//    gNameSpaceManager->CreateRootNameSpace(namespace);
      nsAutoString nameSpace(kXULNameSpace);
      gNameSpaceManager->RegisterNameSpace(nameSpace, nameSpaceID);
    } else {
      NS_ASSERTION(0, "failed to create xul atoms namespace manager");
    }

    // now register the atoms
#define XUL_ATOM(_name, _value) _name = NS_NewAtom(_value);
#include "nsXULAtomList.h"
#undef XUL_ATOM
  }
  ++gRefCnt;
}

void nsXULAtoms::ReleaseAtoms() {

  NS_PRECONDITION(gRefCnt != 0, "bad release of xul atoms");
  if (--gRefCnt == 0) {
#define XUL_ATOM(_name, _value) NS_RELEASE(_name);
#include "nsXULAtomList.h"
#undef XUL_ATOM

    NS_IF_RELEASE(gNameSpaceManager);
  }
}
