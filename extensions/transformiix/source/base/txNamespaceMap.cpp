/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txNamespaceMap.h"
#include "txAtoms.h"
#include "txXPathNode.h"

txNamespaceMap::txNamespaceMap()
{
}

txNamespaceMap::txNamespaceMap(const txNamespaceMap& aOther)
    : mPrefixes(aOther.mPrefixes)
{
    mNamespaces = aOther.mNamespaces; //bah! I want a copy-constructor!
}

nsresult
txNamespaceMap::addNamespace(nsIAtom* aPrefix, const nsAString& aNamespaceURI)
{
    nsIAtom* prefix = aPrefix == txXMLAtoms::_empty ? 0 : aPrefix;

    PRInt32 nsId;
    if (!prefix && aNamespaceURI.IsEmpty()) {
        nsId = kNameSpaceID_None;
    }
    else {
        nsId = txNamespaceManager::getNamespaceID(aNamespaceURI);
        NS_ENSURE_FALSE(nsId == kNameSpaceID_Unknown, NS_ERROR_FAILURE);
    }

    // Check if the mapping already exists
    PRInt32 index = mPrefixes.IndexOf(prefix);
    if (index >= 0) {
        mNamespaces.ReplaceElementAt(NS_INT32_TO_PTR(nsId), index);

        return NS_OK;
    }

    // New mapping
    if (!mPrefixes.AppendObject(prefix)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    if (!mNamespaces.AppendElement(NS_INT32_TO_PTR(nsId))) {
        mPrefixes.RemoveObjectAt(mPrefixes.Count() - 1);

        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

PRInt32
txNamespaceMap::lookupNamespace(nsIAtom* aPrefix)
{
    if (aPrefix == txXMLAtoms::xml) {
        return kNameSpaceID_XML;
    }

    nsIAtom* prefix = aPrefix == txXMLAtoms::_empty ? 0 : aPrefix;

    PRInt32 index = mPrefixes.IndexOf(prefix);
    if (index >= 0) {
        return NS_PTR_TO_INT32(mNamespaces.SafeElementAt(index));
    }

    if (!prefix) {
        return kNameSpaceID_None;
    }

    return kNameSpaceID_Unknown;
}

PRInt32
txNamespaceMap::lookupNamespace(const nsAString& aPrefix)
{
    nsCOMPtr<nsIAtom> prefix = do_GetAtom(aPrefix);

    return lookupNamespace(prefix);
}

PRInt32
txNamespaceMap::lookupNamespaceWithDefault(const nsAString& aPrefix)
{
    nsCOMPtr<nsIAtom> prefix = do_GetAtom(aPrefix);
    if (prefix != txXSLTAtoms::_poundDefault) {
        return lookupNamespace(prefix);
    }

    return lookupNamespace(nsnull);
}
