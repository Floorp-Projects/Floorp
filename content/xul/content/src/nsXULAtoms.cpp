/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 *
 */

/*

  Back-end for commonly used XUL atoms.

 */

#include "nsXULAtoms.h"

#ifdef NS_XULATOM
#undef NS_XULATOM
#endif

#define NS_XULATOM(__atom) nsIAtom* nsXULAtoms::__atom
#include "nsXULAtoms.inc"

nsrefcnt nsXULAtoms::gRefCnt = 0;
nsIAtom* nsXULAtoms::Template;

nsrefcnt
nsXULAtoms::AddRef()
{
    if (++gRefCnt == 1) {
#undef NS_XULATOM
#define NS_XULATOM(__atom) __atom = NS_NewAtom(#__atom)
#include "nsXULAtoms.inc"

        Template = NS_NewAtom("template");
    }

    return gRefCnt;
}


nsrefcnt
nsXULAtoms::Release()
{
    if (--gRefCnt == 0) {
#undef NS_XULATOM
#define NS_XULATOM(__atom) NS_RELEASE(__atom)
#include "nsXULAtoms.inc"

        NS_RELEASE(Template);
    }

    return gRefCnt;
}

