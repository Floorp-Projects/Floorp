/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Rod Spears (rods@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsSVGAtoms.h"
#include "nsLayoutCID.h"

static const char kSVGNameSpace[] = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.svg";

PRInt32  nsSVGAtoms::nameSpaceID;

// define storage for all atoms
#define SVG_ATOM(_name, _value) nsIAtom* nsSVGAtoms::_name;
#include "nsSVGAtomList.h"
#undef SVG_ATOM


static nsrefcnt gRefCnt = 0;
static nsINameSpaceManager* gNameSpaceManager;

void nsSVGAtoms::AddRefAtoms() {

  if (gRefCnt == 0) {
    /* SVG Atoms registers the SVG name space ID because it's a convenient
       place to do this, if you don't want a permanent, "well-known" ID.
    */

    NS_DEFINE_CID(kNameSpaceManagerCID, NS_NAMESPACEMANAGER_CID);
    nsCOMPtr<nsINameSpaceManager> nsmgr =
      do_CreateInstance(kNameSpaceManagerCID);

    if (nsmgr) {
      nsmgr->RegisterNameSpace(NS_ConvertASCIItoUCS2(kSVGNameSpace),
                               nameSpaceID);

      gNameSpaceManager = nsmgr;
      NS_ADDREF(gNameSpaceManager);
    } else {
      NS_ASSERTION(0, "failed to create SVG atoms namespace manager");
    }

    // now register the atoms
#define SVG_ATOM(_name, _value) _name = NS_NewAtom(_value);
#include "nsSVGAtomList.h"
#undef SVG_ATOM
  }
  ++gRefCnt;
}

void nsSVGAtoms::ReleaseAtoms() {

  NS_PRECONDITION(gRefCnt != 0, "bad release of SVG atoms");
  if (--gRefCnt == 0) {
#define SVG_ATOM(_name, _value) NS_RELEASE(_name);
#include "nsSVGAtomList.h"
#undef SVG_ATOM

    NS_IF_RELEASE(gNameSpaceManager);
  }
}
