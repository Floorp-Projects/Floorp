/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
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

#include "InterfaceInitFuncs.h"

#include "nsHyperTextAccessible.h"
#include "nsMai.h"
#include "nsMaiHyperlink.h"

extern "C" {

static AtkHyperlink*
getLinkCB(AtkHypertext *aText, gint aLinkIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return nsnull;

    nsHyperTextAccessible* hyperText = accWrap->AsHyperText();
    NS_ENSURE_TRUE(hyperText, nsnull);

    nsAccessible* hyperLink = hyperText->GetLinkAt(aLinkIndex);
    if (!hyperLink)
        return nsnull;

    AtkObject* hyperLinkAtkObj = nsAccessibleWrap::GetAtkObject(hyperLink);
    nsAccessibleWrap *accChild = GetAccessibleWrap(hyperLinkAtkObj);
    NS_ENSURE_TRUE(accChild, nsnull);

    MaiHyperlink *maiHyperlink = accChild->GetMaiHyperlink();
    NS_ENSURE_TRUE(maiHyperlink, nsnull);
    return maiHyperlink->GetAtkHyperlink();
}

static gint
getLinkCountCB(AtkHypertext *aText)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return -1;

    nsHyperTextAccessible* hyperText = accWrap->AsHyperText();
    NS_ENSURE_TRUE(hyperText, -1);

    return hyperText->GetLinkCount();
}

static gint
getLinkIndexCB(AtkHypertext *aText, gint aCharIndex)
{
    nsAccessibleWrap *accWrap = GetAccessibleWrap(ATK_OBJECT(aText));
    if (!accWrap)
        return -1;

    nsHyperTextAccessible* hyperText = accWrap->AsHyperText();
    NS_ENSURE_TRUE(hyperText, -1);

    PRInt32 index = -1;
    nsresult rv = hyperText->GetLinkIndexAtOffset(aCharIndex, &index);
    NS_ENSURE_SUCCESS(rv, -1);

    return index;
}
}

void
hypertextInterfaceInitCB(AtkHypertextIface* aIface)
{
  NS_ASSERTION(aIface, "no interface!");
  if (NS_UNLIKELY(!aIface))
    return;

  aIface->get_link = getLinkCB;
  aIface->get_n_links = getLinkCountCB;
  aIface->get_link_index = getLinkIndexCB;
}
