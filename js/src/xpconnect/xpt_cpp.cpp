/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* InterfaceInfo support code - some temporary. */

#include <string.h>
#include <stdlib.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "xpt_struct.h"
#include "xpt_cpp.h"

// declare this in the .cpp file for now
class InterfaceInfoImpl : public nsIInterfaceInfo
{
    NS_DECL_ISUPPORTS;

    NS_IMETHOD GetName(char** name); // returns IAllocatator alloc'd copy
    NS_IMETHOD GetIID(nsIID** iid);  // returns IAllocatator alloc'd copy

    NS_IMETHOD GetParent(nsIInterfaceInfo** parent);

    // these include counts of parents
    NS_IMETHOD GetMethodCount(uint16* count);
    NS_IMETHOD GetConstantCount(uint16* count);

    // These include methods and constants of parents.
    // There do *not* make copies ***explicit bending of XPCOM rules***
    NS_IMETHOD GetMethodInfo(uint16 index, const nsXPTMethodInfo** info);
    NS_IMETHOD GetConstant(uint16 index, const nsXPTConstant** constant);

public:
    InterfaceInfoImpl(XPTInterfaceDirectoryEntry* entry,
                      InterfaceInfoImpl* parent);
    virtual ~InterfaceInfoImpl();

private:
    XPTInterfaceDirectoryEntry* mEntry;
    InterfaceInfoImpl* mParent;
    uint16 mMethodBaseIndex;
    uint16 mMethodCount;
    uint16 mConstantBaseIndex;
    uint16 mConstantCount;
};

// declare this in the .cpp file for now
class InterfaceInfoManagerImpl : public nsIInterfaceInfoManager
{
    NS_DECL_ISUPPORTS;

    // nsIInformationInfo management services
    NS_IMETHOD GetInfoForIID(const nsIID* iid, nsIInterfaceInfo** info);
    NS_IMETHOD GetInfoForName(const char* name, nsIInterfaceInfo** info);

    // name <-> IID mapping services
    NS_IMETHOD GetIIDForName(const char* name, nsIID** iid);
    NS_IMETHOD GetNameForIID(const nsIID* iid, char** name);

public:
    InterfaceInfoManagerImpl();
    ~InterfaceInfoManagerImpl();

    static InterfaceInfoManagerImpl* GetInterfaceInfoManager();
    static nsIAllocator* GetAllocator(InterfaceInfoManagerImpl* iim = NULL);
private:
    PRBool BuildInterfaceForEntry(uint16 index);

private:
    // bogus implementation...
    InterfaceInfoImpl** mInfoArray;
    nsIAllocator* mAllocator;
};

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/
// hacked hardcoded simulation...

XPTParamDescriptor ResultParam[] = {
    {XPT_PD_OUT, {TD_UINT32,0}}
};

XPTParamDescriptor QueryInterfaceParams[2] = {
    {XPT_PD_IN, {TD_PNSIID|XPT_TDP_POINTER|XPT_TDP_REFERENCE,0}},
    {XPT_PD_OUT|XPT_PD_RETVAL, {XPT_TDP_POINTER|TD_INTERFACE_IS_TYPE,0}}
};

XPTParamDescriptor TestParams[3] = {
    {XPT_PD_IN, {TD_INT32,0}},
    {XPT_PD_IN, {TD_INT32,0}},
    {XPT_PD_OUT|XPT_PD_RETVAL, {TD_UINT32,0}}
};

XPTMethodDescriptor nsISupportsMethods[3] = {
    {0, "QueryInterface", 2, QueryInterfaceParams, ResultParam},
    {0, "AddRef", 0, NULL, ResultParam},
    {0, "Release", 0, NULL, ResultParam}
};

XPTMethodDescriptor nsITestXPCFooMethods[2] = {
    {0, "Test", 3, TestParams, ResultParam},
    {0, "Test2", 0, NULL, ResultParam}
};

XPTConstDescriptor  nsITestXPCFooConstants[3] = {
    {"five", {TD_INT32,0}, 5},
    {"six", {TD_INT32,0}, 6},
    {"seven", {TD_INT32,0}, 7}
};

XPTInterfaceDescriptor nsISupportsInterfaceDescriptor =
    {NULL, 3, nsISupportsMethods, 0, NULL};

XPTInterfaceDescriptor nsITestXPCFooInterfaceDescriptor =
    {NULL, 2, nsITestXPCFooMethods, 3, nsITestXPCFooConstants};

XPTInterfaceDescriptor nsITestXPCFoo2InterfaceDescriptor =
    {NULL, 0, NULL, 0, NULL};

XPTInterfaceDirectoryEntry InterfaceDirectoryEntryTable[] = {
 {NS_ISUPPORTS_IID, "nsISupports", "", &nsISupportsInterfaceDescriptor},
 {NS_ITESTXPC_FOO_IID, "nsITestXPCFoo", "", &nsITestXPCFooInterfaceDescriptor},
 {NS_ITESTXPC_FOO2_IID, "nsITestXPCFoo2", "", &nsITestXPCFoo2InterfaceDescriptor}
};

#define ENTRY_COUNT (sizeof(InterfaceDirectoryEntryTable)/sizeof(InterfaceDirectoryEntryTable[0]))
#define TABLE_INDEX(p) ((p)-InterfaceDirectoryEntryTable)

static BogusTableInit()
{
    nsITestXPCFooInterfaceDescriptor.parent_interface =
        &InterfaceDirectoryEntryTable[0];

    nsITestXPCFoo2InterfaceDescriptor.parent_interface =
        &InterfaceDirectoryEntryTable[1];
}

/***************************************************************************/
/***************************************************************************/

nsIInterfaceInfo*
nsXPTParamInfo::GetInterface() const
{
    NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE,"not an interface");

    nsIInterfaceInfoManager* mgr;
    if(!(mgr = InterfaceInfoManagerImpl::GetInterfaceInfoManager()))
        return NULL;

    nsIInterfaceInfo* info;
    // not optimal!
    mgr->GetInfoForIID(&InterfaceDirectoryEntryTable[type.type.interface].iid,
                       &info);
    NS_RELEASE(mgr);
    return info;
}

nsIID*
nsXPTParamInfo::GetInterfaceIID() const
{
    NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE,"not an interface");
    return &InterfaceDirectoryEntryTable[type.type.interface].iid;
}

/***************************************************************************/
// VERY simple implementations...

NS_IMPL_ISUPPORTS(InterfaceInfoManagerImpl, NS_IINTERFACEINFO_MANAGER_IID)

XPC_PUBLIC_API(nsIInterfaceInfoManager*)
XPT_GetInterfaceInfoManager()
{
    return InterfaceInfoManagerImpl::GetInterfaceInfoManager();
}

// static
InterfaceInfoManagerImpl*
InterfaceInfoManagerImpl::GetInterfaceInfoManager()
{
    static InterfaceInfoManagerImpl* impl = NULL;
    if(!impl)
    {
        impl = new InterfaceInfoManagerImpl();
        // XXX ought to check for properly formed impl here..
    }
    if(impl)
        NS_ADDREF(impl);
    return impl;
}

// static
nsIAllocator*
InterfaceInfoManagerImpl::GetAllocator(InterfaceInfoManagerImpl* iim /*= NULL*/)
{
    nsIAllocator* al;
    InterfaceInfoManagerImpl* iiml = iim;

    if(!iiml && !(iiml = GetInterfaceInfoManager()))
        return NULL;
    if(NULL != (al = iiml->mAllocator))
        NS_ADDREF(al);
    if(!iim)
        NS_RELEASE(iiml);
    return al;
}


static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);

InterfaceInfoManagerImpl::InterfaceInfoManagerImpl()
    :   mInfoArray(NULL),
        mAllocator(NULL)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    BogusTableInit();

    mInfoArray = (InterfaceInfoImpl**) calloc(ENTRY_COUNT,sizeof(void*));

    nsServiceManager::GetService(kAllocatorCID,
                                 kIAllocatorIID,
                                 (nsISupports **)&mAllocator);
}

InterfaceInfoManagerImpl::~InterfaceInfoManagerImpl()
{
    // let the singleton leak
}

PRBool
InterfaceInfoManagerImpl::BuildInterfaceForEntry(uint16 i)
{
    XPTInterfaceDirectoryEntry *parent_interface =
        InterfaceDirectoryEntryTable[i].interface_descriptor->parent_interface;
    uint16 parent_index = 0;

    if(parent_interface)
    {
        parent_index = TABLE_INDEX(parent_interface);
        if(!mInfoArray[parent_index] && ! BuildInterfaceForEntry(parent_index))
            return PR_FALSE;
    }
    mInfoArray[i] = new InterfaceInfoImpl(&InterfaceDirectoryEntryTable[i],
                                          parent_interface ?
                                              mInfoArray[parent_index] : NULL);
    return (PRBool) mInfoArray[i];
}

NS_IMETHODIMP
InterfaceInfoManagerImpl::GetInfoForIID(const nsIID* iid, nsIInterfaceInfo** info)
{
    for(int i = 0; i < ENTRY_COUNT;i++)
    {
        XPTInterfaceDirectoryEntry* entry = &InterfaceDirectoryEntryTable[i];
        if(iid->Equals(entry->iid))
        {
            if(!mInfoArray[i] && !BuildInterfaceForEntry(i))
                break;
            *info = mInfoArray[i];
            NS_ADDREF(*info);
            return NS_OK;
        }
    }
    *info = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InterfaceInfoManagerImpl::GetInfoForName(const char* name, nsIInterfaceInfo** info)
{
    for(int i = 0; i < ENTRY_COUNT;i++)
    {
        XPTInterfaceDirectoryEntry* entry = &InterfaceDirectoryEntryTable[i];
        if(!strcmp(name, entry->name))
        {
            if(!mInfoArray[i] && !BuildInterfaceForEntry(i))
                break;
            *info = mInfoArray[i];
            NS_ADDREF(*info);
            return NS_OK;
        }
    }
    *info = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InterfaceInfoManagerImpl::GetIIDForName(const char* name, nsIID** iid)
{
    for(int i = 0; i < ENTRY_COUNT;i++)
    {
        XPTInterfaceDirectoryEntry* entry = &InterfaceDirectoryEntryTable[i];
        if(!strcmp(name, entry->name))
        {
            nsIID* p;
            if(!(p = (nsIID*)mAllocator->Alloc(sizeof(nsIID))))
                break;
            memcpy(p, &entry->iid, sizeof(nsIID));
            *iid = p;
            return NS_OK;
        }
    }
    *iid = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InterfaceInfoManagerImpl::GetNameForIID(const nsIID* iid, char** name)
{
    for(int i = 0; i < ENTRY_COUNT;i++)
    {
        XPTInterfaceDirectoryEntry* entry = &InterfaceDirectoryEntryTable[i];
        if(iid->Equals(entry->iid))
        {
            char* p;
            int len = strlen(entry->name)+1;
            if(!(p = (char*)mAllocator->Alloc(len)))
                break;
            memcpy(p, &entry->name, len);
            *name = p;
            return NS_OK;
        }
    }
    *name = NULL;
    return NS_ERROR_FAILURE;
}

/***************************************************************************/
NS_IMPL_ISUPPORTS(InterfaceInfoImpl, NS_IINTERFACEINFO_IID)

InterfaceInfoImpl::InterfaceInfoImpl(XPTInterfaceDirectoryEntry* entry,
                                     InterfaceInfoImpl* parent)
    :   mEntry(entry),
        mParent(parent)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
    if(mParent)
        NS_ADDREF(mParent);
    if(mParent)
    {
        mMethodBaseIndex = mParent->mMethodBaseIndex + mParent->mMethodCount;
        mConstantBaseIndex = mParent->mConstantBaseIndex + mParent->mConstantCount;
    }
    else
        mMethodBaseIndex = mConstantBaseIndex = 0;

    mMethodCount   = mEntry->interface_descriptor->num_methods;
    mConstantCount = mEntry->interface_descriptor->num_constants;
}

InterfaceInfoImpl::~InterfaceInfoImpl()
{
    if(mParent)
        NS_RELEASE(mParent);
}

NS_IMETHODIMP
InterfaceInfoImpl::GetName(char** name)
{
    NS_PRECONDITION(name, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = InterfaceInfoManagerImpl::GetAllocator()))
    {
        int len = strlen(mEntry->name)+1;
        char* p = (char*)allocator->Alloc(len);
        NS_RELEASE(allocator);
        if(p)
        {
            memcpy(p, mEntry->name, len);
            *name = p;
            return NS_OK;
        }
    }

    *name = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InterfaceInfoImpl::GetIID(nsIID** iid)
{
    NS_PRECONDITION(iid, "bad param");

    nsIAllocator* allocator;
    if(NULL != (allocator = InterfaceInfoManagerImpl::GetAllocator()))
    {
        nsIID* p = (nsIID*)allocator->Alloc(sizeof(nsIID));
        NS_RELEASE(allocator);
        if(p)
        {
            memcpy(p, &mEntry->iid, sizeof(nsIID));
            *iid = p;
            return NS_OK;
        }
    }

    *iid = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InterfaceInfoImpl::GetParent(nsIInterfaceInfo** parent)
{
    NS_PRECONDITION(parent, "bad param");
    if(mParent)
    {
        NS_ADDREF(mParent);
        *parent = mParent;
        return NS_OK;
    }
    *parent = NULL;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InterfaceInfoImpl::GetMethodCount(uint16* count)
{
    NS_PRECONDITION(count, "bad param");
    *count = mMethodBaseIndex + mMethodCount;
    return NS_OK;
}

NS_IMETHODIMP
InterfaceInfoImpl::GetConstantCount(uint16* count)
{
    NS_PRECONDITION(count, "bad param");
    *count = mConstantBaseIndex + mConstantCount;
    return NS_OK;

}

NS_IMETHODIMP
InterfaceInfoImpl::GetMethodInfo(uint16 index, const nsXPTMethodInfo** info)
{
    NS_PRECONDITION(info, "bad param");
    if(index < mMethodBaseIndex)
        return mParent->GetMethodInfo(index, info);

    if(index >= mMethodBaseIndex + mMethodCount)
    {
        NS_PRECONDITION(0, "bad param");
        *info = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *info = NS_STATIC_CAST(nsXPTMethodInfo*, &mEntry->interface_descriptor->method_descriptors[index-mMethodBaseIndex]);
    return NS_OK;

}

NS_IMETHODIMP
InterfaceInfoImpl::GetConstant(uint16 index, const nsXPTConstant** constant)
{
    NS_PRECONDITION(constant, "bad param");
    if(index < mConstantBaseIndex)
        return mParent->GetConstant(index, constant);

    if(index >= mConstantBaseIndex + mConstantCount)
    {
        NS_PRECONDITION(0, "bad param");
        *constant = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *constant = NS_STATIC_CAST(nsXPTConstant*,&mEntry->interface_descriptor->const_descriptors[index-mConstantBaseIndex]);
    return NS_OK;
}
