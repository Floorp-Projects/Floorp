/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
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

/* Library-private header for Interface Info extras system. */

#ifndef iixprivate_h___
#define iixprivate_h___

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsSupportsArray.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsWeakReference.h"
#include "nsIGenericFactory.h"
#include "nsVariant.h"

#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xpt_struct.h"
#include "xptinfo.h"
#include "xptcall.h"

#include "nsIGenericInterfaceInfoSet.h"
#include "nsIScriptableInterfaceInfo.h"

/***************************************************************************/

class nsGenericInterfaceInfoSet : public nsIGenericInterfaceInfoSet,
                                  public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER
    NS_DECL_NSIGENERICINTERFACEINFOSET

    nsGenericInterfaceInfoSet();
    virtual ~nsGenericInterfaceInfoSet();

    XPTArena* GetArena()
    {
        return mArena;
    }

    const XPTTypeDescriptor* GetAdditionalTypeAt(PRUint16 aIndex)
    {
        NS_ASSERTION(aIndex < (PRUint16) mAdditionalTypes.Count(), "bad index");
        return (const XPTTypeDescriptor*) mAdditionalTypes.ElementAt(aIndex);
    }

    nsIInterfaceInfo* InfoAtNoAddRef(PRUint16 aIndex)
    {
        NS_ASSERTION(aIndex < (PRUint16) mInterfaces.Count(), "bad index");
        return (nsIInterfaceInfo*) ClearOwnedFlag(mInterfaces.ElementAt(aIndex));
    }

private:
    nsresult IndexOfIID(const nsIID & aIID, PRUint16 *_retval);
    nsresult IndexOfName(const char* aName, PRUint16 *_retval);

    void* SetOwnedFlag(void* p)
    {
        NS_ASSERTION(sizeof(void *) == sizeof(long),
                     "The size of a pointer != size of long!");
        return (void*) ((long)p | 1);
    }
    void* ClearOwnedFlag(void* p)
    {
        return (void*) ((long)p & ~(long)1);
    }
    PRBool CheckOwnedFlag(void* p)
    {
        return (PRBool) ((long)p & (long)1);
    }

private:
    nsVoidArray mInterfaces;
    nsVoidArray mAdditionalTypes;
    XPTArena* mArena;
};

/***************************************************************************/

class nsGenericInterfaceInfo : public nsIGenericInterfaceInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFO
    NS_DECL_NSIGENERICINTERFACEINFO

    nsGenericInterfaceInfo(nsGenericInterfaceInfoSet* aSet,
                           const char *aName, 
                           const nsIID & aIID, 
                           nsIInterfaceInfo* aParent,
                           PRUint8 aFlags); 
    virtual ~nsGenericInterfaceInfo()
    {
    }

private:
    nsGenericInterfaceInfo(); // not implemented

    const XPTTypeDescriptor* GetPossiblyNestedType(const XPTParamDescriptor* param)
    {
        const XPTTypeDescriptor* td = &param->type;
        while(XPT_TDP_TAG(td->prefix) == TD_ARRAY)
            td = mSet->GetAdditionalTypeAt(td->type.additional_type);
        return td;
    }

    const XPTTypeDescriptor* GetTypeInArray(const XPTParamDescriptor* param,
                                            PRUint16 dimension)
    {
        const XPTTypeDescriptor* td = &param->type;
        for(PRUint16 i = 0; i < dimension; i++)
        {
            NS_ASSERTION(XPT_TDP_TAG(td->prefix) == TD_ARRAY, "bad dimension");
            td = mSet->GetAdditionalTypeAt(td->type.additional_type);
        }
        return td;
    }

private:
    char* mName;
    nsIID mIID;
    nsVoidArray mMethods;
    nsVoidArray mConstants;
    nsGenericInterfaceInfoSet* mSet;
    nsIInterfaceInfo* mParent; // weak reference (it must be held in set table)
    PRUint16 mMethodBaseIndex;
    PRUint16 mConstantBaseIndex;
    PRUint8 mFlags;
};

/***************************************************************************/

class nsScriptableInterfaceInfo : public nsIScriptableInterfaceInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEINTERFACEINFO

    static nsresult Create(nsIInterfaceInfo* aInfo,
                           nsIScriptableInterfaceInfo** aResult);

    nsScriptableInterfaceInfo();
    nsScriptableInterfaceInfo(nsIInterfaceInfo* aInfo);
    virtual ~nsScriptableInterfaceInfo();

private:
    nsCOMPtr<nsIInterfaceInfo> mInfo;
};

/***************************************************************************/

#endif /* iixprivate_h___ */
