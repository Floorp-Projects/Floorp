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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "nsXPathNSResolver.h"
#include "nsIDOMClassInfo.h"
#include "nsDOMString.h"

NS_IMPL_ADDREF(nsXPathNSResolver)
NS_IMPL_RELEASE(nsXPathNSResolver)
NS_INTERFACE_MAP_BEGIN(nsXPathNSResolver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathNSResolver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathNSResolver)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPathNSResolver)
NS_INTERFACE_MAP_END

nsXPathNSResolver::nsXPathNSResolver(nsIDOMNode* aNode)
{
    mNode = do_QueryInterface(aNode);
    NS_ASSERTION(mNode, "Need a node to resolve namespaces.");
}

nsXPathNSResolver::~nsXPathNSResolver()
{
}

NS_IMETHODIMP
nsXPathNSResolver::LookupNamespaceURI(const nsAString & aPrefix,
                                      nsAString & aResult)
{
    if (aPrefix.EqualsLiteral("xml")) {
        aResult.AssignLiteral("http://www.w3.org/XML/1998/namespace");

        return NS_OK;
    }

    if (!mNode) {
        SetDOMStringToNull(aResult);

        return NS_OK;
    }

    return mNode->LookupNamespaceURI(aPrefix, aResult);
}
