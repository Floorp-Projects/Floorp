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

#ifndef __MAI_HYPERLINK_H__
#define __MAI_HYPERLINK_H__


#include "nsString.h"
#include "nsMai.h"
#include "nsIAccessibleHyperLink.h"

struct _AtkHyperlink;
typedef struct _AtkHyperlink                      AtkHyperlink;

/*
 * MaiHyperlink is a auxiliary class for MaiInterfaceHyperText.
 */

class MaiHyperlink : public nsAccessNodeWrap
{
public:
    MaiHyperlink(nsIAccessibleHyperLink *aAcc,
                 nsIDOMNode *aNode, nsIWeakReference* aShell);
    ~MaiHyperlink();
    NS_IMETHOD GetUniqueID(void **aUniqueID);

public:
    AtkHyperlink *GetAtkHyperlink(void);
    nsIAccessibleHyperLink *GetAccHyperlink(void) {
        return mHyperlink;
    }

protected:
    nsCOMPtr<nsIAccessibleHyperLink> mHyperlink;
    AtkHyperlink *mMaiAtkHyperlink;
public:
    static nsresult Initialize(AtkHyperlink *aObj, MaiHyperlink *aClass);
};
#endif /* __MAI_HYPERLINK_H__ */
