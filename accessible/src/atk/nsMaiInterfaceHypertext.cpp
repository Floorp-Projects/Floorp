/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s):
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

#include "nsMaiInterfaceHypertext.h"
#include "nsIAccessibleDocument.h"
#include "nsPIAccessNode.h"

G_BEGIN_DECLS

static void interfaceInitCB(AtkHypertextIface *aIface);

/* hypertext interface callbacks */
static AtkHyperlink *getLinkCB(AtkHypertext *aText, gint aLinkIndex);
static gint getLinkCountCB(AtkHypertext *aText);
static gint getLinkIndexCB(AtkHypertext *aText, gint aCharIndex);

G_END_DECLS

MaiInterfaceHypertext::MaiInterfaceHypertext(nsAccessibleWrap *aAccWrap,
                                             nsIWeakReference* aShell):
    MaiInterface(aAccWrap),
    mWeakShell(aShell)
{
}

MaiInterfaceHypertext::~MaiInterfaceHypertext()
{
    mWeakShell = nsnull;
}

MaiInterfaceType
MaiInterfaceHypertext::GetType()
{
    return MAI_INTERFACE_HYPERTEXT;
}

nsresult
MaiInterfaceHypertext::GetWeakShell(nsIWeakReference **aWeakShell)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mWeakShell) {
        *aWeakShell = mWeakShell;
        NS_IF_ADDREF(*aWeakShell);
        rv = NS_OK;
    }
    else
        *aWeakShell = nsnull;
    return rv;
}

const GInterfaceInfo *
MaiInterfaceHypertext::GetInterfaceInfo()
{
    static const GInterfaceInfo atk_if_hypertext_info = {
        (GInterfaceInitFunc)interfaceInitCB,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };
    return &atk_if_hypertext_info;
}

/* statics */

void
interfaceInitCB(AtkHypertextIface *aIface)
{
    g_return_if_fail(aIface != NULL);

    aIface->get_link = getLinkCB;
    aIface->get_n_links = getLinkCountCB;
    aIface->get_link_index = getLinkIndexCB;
}

AtkHyperlink *
getLinkCB(AtkHypertext *aText, gint aLinkIndex)
{
    nsresult rv;
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, nsnull);

    MaiInterfaceHypertext *maiHypertext =
        NS_STATIC_CAST(MaiInterfaceHypertext *,
                       accWrap->GetMaiInterface(MAI_INTERFACE_HYPERTEXT));
    NS_ENSURE_TRUE(accWrap, nsnull);

    nsCOMPtr<nsIWeakReference> weakShell;
    rv = maiHypertext->GetWeakShell(getter_AddRefs(weakShell));
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIAccessibleHyperText> accHyperlink;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleHyperText),
                            getter_AddRefs(accHyperlink));
    NS_ENSURE_TRUE(accHyperlink, nsnull);

    nsCOMPtr<nsIAccessibleHyperLink> hyperLink;
    rv = accHyperlink->GetLink(aLinkIndex, getter_AddRefs(hyperLink));
    if (NS_FAILED(rv) || !hyperLink)
        return nsnull;

    // The MaiHyperlink and its nsIAccessibleHyperlink are in the same cache.
    // we take hyperlink->get() as Id for the MaiHyperlink.
    // release our ref to the previous one

    nsCOMPtr<nsIAccessibleDocument> accessibleDoc;
    nsAccessNode::GetDocAccessibleFor(weakShell,
                                      getter_AddRefs(accessibleDoc));
    if (!accessibleDoc) {
        NS_WARNING("No accessible document for this presshell");
        return nsnull;
    }
    nsCOMPtr<nsIAccessNode> maiNode;
    accessibleDoc->GetCachedAccessNode(NS_STATIC_CAST(void*, hyperLink.get()),
                                       getter_AddRefs(maiNode));
    // if the maiHyperlink is not in cache, create it.
    if (!maiNode) {
        maiNode = new MaiHyperlink(hyperLink, nsnull, weakShell);
        if (!maiNode) {
            NS_WARNING("OUT OF MEMORY");
            return nsnull;
        }
        nsCOMPtr<nsPIAccessNode> pMaiNode = do_QueryInterface(maiNode);
        pMaiNode->Init();  // add to cache.
    }

    // we can get AtkHyperlink from the MaiHyperlink
    nsIAccessNode *tmpNode = maiNode;
    MaiHyperlink *maiHyperlink = NS_STATIC_CAST(MaiHyperlink *, tmpNode);

    /* we should not addref the atkhyperlink because we are "get" not "ref" */
    return maiHyperlink->GetAtkHyperlink();
}

gint
getLinkCountCB(AtkHypertext *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleHyperText> accHyperlink;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleHyperText),
                            getter_AddRefs(accHyperlink));
    NS_ENSURE_TRUE(accHyperlink, -1);

    PRInt32 count = -1;
    nsresult rv = accHyperlink->GetLinks(&count);
    NS_ENSURE_SUCCESS(rv, -1);

    return count;
}

gint
getLinkIndexCB(AtkHypertext *aText, gint aCharIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    NS_ENSURE_TRUE(accWrap, -1);

    nsCOMPtr<nsIAccessibleHyperText> accHyperlink;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleHyperText),
                            getter_AddRefs(accHyperlink));
    NS_ENSURE_TRUE(accHyperlink, -1);

    PRInt32 index = -1;
    nsresult rv = accHyperlink->GetLinkIndex(aCharIndex, &index);
    NS_ENSURE_SUCCESS(rv, -1);

    return index;
}
