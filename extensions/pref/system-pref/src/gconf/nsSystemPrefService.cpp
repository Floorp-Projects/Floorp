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
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
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

#include <glib.h>
#include <glib-object.h>

#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "nsString.h"
#include "nsSystemPrefLog.h"
#include "nsSystemPrefService.h"

/*************************************************************************
 * The strange thing here is that we load the gconf library manually and 
 * search the function pointers we need. If that process fails, no gconf
 * support is available in mozilla. The aim is to make mozilla independent
 * on gconf, in both compile time and run time.
 ************************************************************************/

//gconf types
extern "C" {

    typedef enum {
        GCONF_VALUE_INVALID,
        GCONF_VALUE_STRING,
        GCONF_VALUE_INT,
        GCONF_VALUE_FLOAT,
        GCONF_VALUE_BOOL,
        GCONF_VALUE_SCHEMA,

        GCONF_VALUE_LIST,
        GCONF_VALUE_PAIR

    }GConfValueType;

    typedef struct {
        GConfValueType type;
    }GConfValue;

    typedef void * (*GConfClientGetDefaultType) (void);
    typedef PRBool (*GConfClientGetBoolType) (void *client, const gchar *key,
                                              GError **err);
    typedef gchar* (*GConfClientGetStringType) (void *client, const gchar *key,
                                                GError **err);
    typedef PRInt32 (*GConfClientGetIntType) (void *client, const gchar *key,
                                              GError **err);
    typedef  void (*GConfClientNotifyFuncType) (void* client, guint cnxn_id,
                                                void *entry, 
                                                gpointer user_data);
    typedef guint (*GConfClientNotifyAddType) (void* client,
                                               const gchar* namespace_section,
                                               GConfClientNotifyFuncType func,
                                               gpointer user_data,
                                               GFreeFunc destroy_notify,
                                               GError** err);
    typedef void (*GConfClientNotifyRemoveType) (void *client,
                                                 guint cnxn);
    typedef void (*GConfClientAddDirType) (void *client,
                                           const gchar *dir,
                                           guint8 preload,
                                           GError **err);
    typedef void (*GConfClientRemoveDirType) (void *client,
                                              const gchar *dir,
                                              GError **err);

    typedef const char* (*GConfEntryGetKeyType) (const void *entry);
    typedef GConfValue* (*GConfEntryGetValueType) (const void *entry);

    typedef const char* (*GConfValueGetStringType) (const GConfValue *value);
    typedef PRInt32 (*GConfValueGetIntType) (const GConfValue *value);
    typedef PRBool (*GConfValueGetBoolType) (const GConfValue *value);

    
    static void gconf_key_listener (void* client, guint cnxn_id,
                                    void *entry, gpointer user_data);
}

struct GConfCallbackData
{
    GConfProxy *proxy;
    void * userData;
    PRUint32 atom;
    PRUint32 notifyId;
};
//////////////////////////////////////////////////////////////////////
// GConPrxoy is a thin wrapper for easy use of gconf funcs. It loads the
// gconf library and initializes the func pointers for later use.
//////////////////////////////////////////////////////////////////////
class GConfProxy
{
public:
    GConfProxy(nsSystemPrefService* aSysPrefService);
    ~GConfProxy();
    PRBool Init();

    nsresult GetBoolPref(const char *aMozKey, PRBool *retval);
    nsresult GetCharPref(const char *aMozKey, char **retval);
    nsresult GetIntPref(const char *aMozKey, PRInt32 *retval);

    nsresult NotifyAdd (PRUint32 aAtom, void *aUserData);
    nsresult NotifyRemove (PRUint32 aAtom, const void *aUserData);

    nsresult GetAtomForMozKey(const char *aMozKey, PRUint32 *aAtom) {
        return GetAtom(aMozKey, 0, aAtom); 
    }
    const char *GetMozKey(PRUint32 aAtom) {
        return GetKey(aAtom, 0); 
    }

    void OnNotify(void *aClient, void * aEntry, PRUint32 aNotifyId,
                  GConfCallbackData *aData);

private:
    void *mGConfClient;
    PRLibrary *mGConfLib;
    PRBool mInitialized;
    nsSystemPrefService *mSysPrefService;

    //listeners
    nsAutoVoidArray *mObservers;

    void InitFuncPtrs();
    //gconf public func ptrs

    //gconf client funcs
    GConfClientGetDefaultType GConfClientGetDefault;
    GConfClientGetBoolType GConfClientGetBool;
    GConfClientGetStringType GConfClientGetString;
    GConfClientGetIntType GConfClientGetInt;
    GConfClientNotifyAddType GConfClientNotifyAdd;
    GConfClientNotifyRemoveType GConfClientNotifyRemove;
    GConfClientAddDirType GConfClientAddDir;
    GConfClientRemoveDirType GConfClientRemoveDir;

    //gconf entry funcs
    GConfEntryGetValueType GConfEntryGetValue;
    GConfEntryGetKeyType GConfEntryGetKey;

    //gconf value funcs
    GConfValueGetBoolType GConfValueGetBool;
    GConfValueGetStringType GConfValueGetString;
    GConfValueGetIntType GConfValueGetInt;

    //pref name translating stuff
    nsresult GetAtom(const char *aKey, PRUint8 aNameType, PRUint32 *aAtom);
    nsresult GetAtomForGConfKey(const char *aGConfKey, PRUint32 *aAtom) \
    {return GetAtom(aGConfKey, 1, aAtom);}
    const char *GetKey(PRUint32 aAtom, PRUint8 aNameType);
    const char *GetGConfKey(PRUint32 aAtom) \
    {return GetKey(aAtom, 1); }
    inline const char *MozKey2GConfKey(const char *aMozKey);

    //const strings
    static const char sPrefGConfKey[];
    static const char sDefaultLibName1[];
    static const char sDefaultLibName2[];
};

struct SysPrefCallbackData {
    nsISupports *observer;
    PRBool bIsWeakRef;
    PRUint32 prefAtom;
};

PRBool PR_CALLBACK
sysPrefDeleteObserver(void *aElement, void *aData) {
    SysPrefCallbackData *pElement =
        NS_STATIC_CAST(SysPrefCallbackData *, aElement);
    NS_RELEASE(pElement->observer);
    nsMemory::Free(pElement);
    return PR_TRUE;
}

NS_IMPL_ISUPPORTS2(nsSystemPrefService, nsIPrefBranch, nsIPrefBranchInternal)

/* public */
nsSystemPrefService::nsSystemPrefService()
    :mInitialized(PR_FALSE),
     mGConf(nsnull),
     mObservers(nsnull)
{
}

nsSystemPrefService::~nsSystemPrefService()
{
    mInitialized = PR_FALSE;

    if (mGConf)
        delete mGConf;
    if (mObservers) {
        (void)mObservers->EnumerateForwards(sysPrefDeleteObserver, nsnull);
        delete mObservers;
    }
}

nsresult
nsSystemPrefService::Init()
{
    if (!gSysPrefLog) {
        gSysPrefLog = PR_NewLogModule("Syspref");
        if (!gSysPrefLog) return NS_ERROR_OUT_OF_MEMORY;
    }

    SYSPREF_LOG(("Init SystemPref Service\n"));
    if (mInitialized)
        return NS_ERROR_FAILURE;

    if (!mGConf) {
        mGConf = new GConfProxy(this);
        if (!mGConf->Init()) {
            delete mGConf;
            mGConf = nsnull;
            return NS_ERROR_FAILURE;
        }
    }

    mInitialized = PR_TRUE;
    return NS_OK;
}

/* readonly attribute string root; */
NS_IMETHODIMP nsSystemPrefService::GetRoot(char * *aRoot)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getPrefType (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::GetPrefType(const char *aPrefName, PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean getBoolPref (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::GetBoolPref(const char *aPrefName, PRBool *_retval)
{
    return mInitialized ?
        mGConf->GetBoolPref(aPrefName, _retval) : NS_ERROR_FAILURE;
}

/* void setBoolPref (in string aPrefName, in long aValue); */
NS_IMETHODIMP nsSystemPrefService::SetBoolPref(const char *aPrefName, PRInt32 aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string getCharPref (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::GetCharPref(const char *aPrefName, char **_retval)
{
    return mInitialized ?
        mGConf->GetCharPref(aPrefName, _retval) : NS_ERROR_FAILURE;
}

/* void setCharPref (in string aPrefName, in string aValue); */
NS_IMETHODIMP nsSystemPrefService::SetCharPref(const char *aPrefName, const char *aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getIntPref (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::GetIntPref(const char *aPrefName, PRInt32 *_retval)
{
    return mInitialized ?
        mGConf->GetIntPref(aPrefName, _retval) : NS_ERROR_FAILURE;
}

/* void setIntPref (in string aPrefName, in long aValue); */
NS_IMETHODIMP nsSystemPrefService::SetIntPref(const char *aPrefName, PRInt32 aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getComplexValue (in string aPrefName, in nsIIDRef aType, [iid_is (aType), retval] out nsQIResult aValue); */
NS_IMETHODIMP nsSystemPrefService::GetComplexValue(const char *aPrefName, const nsIID & aType, void * *aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setComplexValue (in string aPrefName, in nsIIDRef aType, in nsISupports aValue); */
NS_IMETHODIMP nsSystemPrefService::SetComplexValue(const char *aPrefName, const nsIID & aType, nsISupports *aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void clearUserPref (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::ClearUserPref(const char *aPrefName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void lockPref (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::LockPref(const char *aPrefName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean prefHasUserValue (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::PrefHasUserValue(const char *aPrefName, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean prefIsLocked (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::PrefIsLocked(const char *aPrefName, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unlockPref (in string aPrefName); */
NS_IMETHODIMP nsSystemPrefService::UnlockPref(const char *aPrefName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void deleteBranch (in string aStartingAt); */
NS_IMETHODIMP nsSystemPrefService::DeleteBranch(const char *aStartingAt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getChildList (in string aStartingAt, out unsigned long aCount, [array, size_is (aCount), retval] out string aChildArray); */
NS_IMETHODIMP nsSystemPrefService::GetChildList(const char *aStartingAt, PRUint32 *aCount, char ***aChildArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resetBranch (in string aStartingAt); */
NS_IMETHODIMP nsSystemPrefService::ResetBranch(const char *aStartingAt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void addObserver (in string aDomain, in nsIObserver aObserver, in boolean aHoldWeak); */
NS_IMETHODIMP nsSystemPrefService::AddObserver(const char *aDomain, nsIObserver *aObserver, PRBool aHoldWeak)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aDomain);
    NS_ENSURE_ARG_POINTER(aObserver);

    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);

    PRUint32 prefAtom;
    // make sure the pref name is supported
    rv = mGConf->GetAtomForMozKey(aDomain, &prefAtom);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mObservers) {
        mObservers = new nsAutoVoidArray();
        if (mObservers == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    SysPrefCallbackData *pCallbackData = (SysPrefCallbackData *)
        nsMemory::Alloc(sizeof(SysPrefCallbackData));
    if (pCallbackData == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    pCallbackData->bIsWeakRef = aHoldWeak;
    pCallbackData->prefAtom = prefAtom;
    // hold a weak reference to the observer if so requested
    nsCOMPtr<nsISupports> observerRef;
    if (aHoldWeak) {
        nsCOMPtr<nsISupportsWeakReference> weakRefFactory = 
            do_QueryInterface(aObserver);
        if (!weakRefFactory) {
            // the caller didn't give us a object that supports weak reference.
            // ... tell them
            nsMemory::Free(pCallbackData);
            return NS_ERROR_INVALID_ARG;
        }
        observerRef = do_GetWeakReference(weakRefFactory);
    } else {
        observerRef = aObserver;
    }

    rv = mGConf->NotifyAdd(prefAtom, pCallbackData);
    if (NS_FAILED(rv)) {
        nsMemory::Free(pCallbackData);
        return rv;
    }

    pCallbackData->observer = observerRef;
    NS_ADDREF(pCallbackData->observer);

    mObservers->AppendElement(pCallbackData);
    return NS_OK;
}

/* void removeObserver (in string aDomain, in nsIObserver aObserver); */
NS_IMETHODIMP nsSystemPrefService::RemoveObserver(const char *aDomain, nsIObserver *aObserver)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aDomain);
    NS_ENSURE_ARG_POINTER(aObserver);
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);

    if (!mObservers)
        return NS_OK;
    
    PRUint32 prefAtom;
    // make sure the pref name is supported
    rv = mGConf->GetAtomForMozKey(aDomain, &prefAtom);
    NS_ENSURE_SUCCESS(rv, rv);

    // need to find the index of observer, so we can remove it
    PRIntn count = mObservers->Count();
    if (count <= 0)
        return NS_OK;

    PRIntn i;
    SysPrefCallbackData *pCallbackData;
    for (i = 0; i < count; ++i) {
        pCallbackData = (SysPrefCallbackData *)mObservers->ElementAt(i);
        if (pCallbackData) {
            nsCOMPtr<nsISupports> observerRef;
            if (pCallbackData->bIsWeakRef) {
                nsCOMPtr<nsISupportsWeakReference> weakRefFactory =
                    do_QueryInterface(aObserver);
                if (weakRefFactory)
                    observerRef = do_GetWeakReference(aObserver);
            }
            if (!observerRef)
                observerRef = aObserver;

            if (pCallbackData->observer == observerRef &&
                pCallbackData->prefAtom == prefAtom) {
                rv = mGConf->NotifyRemove(prefAtom, pCallbackData);
                if (NS_SUCCEEDED(rv)) {
                    mObservers->RemoveElementAt(i);
                    NS_RELEASE(pCallbackData->observer);
                    nsMemory::Free(pCallbackData);
                }
                return rv;
            }
        }
    }
    return NS_OK;
}

void
nsSystemPrefService::OnPrefChange(PRUint32 aPrefAtom, void *aData)
{
    if (!mInitialized)
        return;

    SysPrefCallbackData *pData = (SysPrefCallbackData *)aData;
    if (pData->prefAtom != aPrefAtom)
        return;

    nsCOMPtr<nsIObserver> observer;
    if (pData->bIsWeakRef) {
        nsCOMPtr<nsIWeakReference> weakRef =
            do_QueryInterface(pData->observer);
        if(weakRef)
            observer = do_QueryReferent(weakRef);
        if (!observer) {
            // this weak referenced observer went away, remove it from the list
            nsresult rv = mGConf->NotifyRemove(aPrefAtom, pData);
            if (NS_SUCCEEDED(rv)) {
                mObservers->RemoveElement(pData);
                NS_RELEASE(pData->observer);
                nsMemory::Free(pData);
            }
            return;
        }
    }
    else
        observer = do_QueryInterface(pData->observer);

    if (observer)
        observer->Observe(NS_STATIC_CAST(nsIPrefBranch *, this),
                          NS_SYSTEMPREF_PREFCHANGE_TOPIC_ID,
                          NS_ConvertUTF8toUCS2(mGConf->GetMozKey(aPrefAtom)).
                          get());
}

/*************************************************************
 *  GConfProxy
 *
 ************************************************************/

struct GConfFuncListType {
    const char *FuncName;
    PRFuncPtr  FuncPtr;
};

struct PrefNamePair {
    const char *mozPrefName;
    const char *gconfPrefName;
};

const char
GConfProxy::sPrefGConfKey[] = "accessibility.unix.gconf2.shared-library";
const char GConfProxy::sDefaultLibName1[] = "libgconf-2.so.4";
const char GConfProxy::sDefaultLibName2[] = "libgconf-2.so";

#define GCONF_FUNCS_POINTER_BEGIN \
    static GConfFuncListType sGConfFuncList[] = {
#define GCONF_FUNCS_POINTER_ADD(func_name) \
    {func_name, nsnull},
#define GCONF_FUNCS_POINTER_END \
    {nsnull, nsnull}, };

GCONF_FUNCS_POINTER_BEGIN
    GCONF_FUNCS_POINTER_ADD("gconf_client_get_default")        // 0
    GCONF_FUNCS_POINTER_ADD("gconf_client_get_bool")       // 1
    GCONF_FUNCS_POINTER_ADD("gconf_client_get_string")     //2
    GCONF_FUNCS_POINTER_ADD("gconf_client_get_int")       //3
    GCONF_FUNCS_POINTER_ADD("gconf_client_notify_add")   //4
    GCONF_FUNCS_POINTER_ADD("gconf_client_notify_remove")   //5
    GCONF_FUNCS_POINTER_ADD("gconf_client_add_dir")   //6
    GCONF_FUNCS_POINTER_ADD("gconf_client_remove_dir")   //7
    GCONF_FUNCS_POINTER_ADD("gconf_entry_get_value")       //8
    GCONF_FUNCS_POINTER_ADD("gconf_entry_get_key")       //9
    GCONF_FUNCS_POINTER_ADD("gconf_value_get_bool")      //10
    GCONF_FUNCS_POINTER_ADD("gconf_value_get_string")     //11
    GCONF_FUNCS_POINTER_ADD("gconf_value_get_int")       //12
GCONF_FUNCS_POINTER_END

/////////////////////////////////////////////////////////////////////////////
// the list is the mapping table, between mozilla prefs and gconf prefs
// It is expected to include all the pref pairs that are related in mozilla
// and gconf. 
//
// Note: the prefs listed here are not neccessarily be read from gconf, they
//       are the prefs that could be read from gconf. Mozilla has another
//       list (see sSysPrefList in nsSystemPref.cpp) that decide which prefs
//       are really read.
//////////////////////////////////////////////////////////////////////////////

static const PrefNamePair sPrefNameMapping[] = {
#include "gconf_pref_list.inc"
    {nsnull, nsnull},
};

PRBool PR_CALLBACK
gconfDeleteObserver(void *aElement, void *aData) {
    nsMemory::Free(aElement);
    return PR_TRUE;
}

GConfProxy::GConfProxy(nsSystemPrefService *aSysPrefService):
    mGConfClient(nsnull),
    mGConfLib(nsnull),
    mInitialized(PR_FALSE),
    mSysPrefService(aSysPrefService),
    mObservers(nsnull)
{
}

GConfProxy::~GConfProxy()
{
    if (mGConfClient)
        g_object_unref(G_OBJECT(mGConfClient));

    if (mObservers) {
        (void)mObservers->EnumerateForwards(gconfDeleteObserver, nsnull);
        delete mObservers;
    }
}

PRBool
GConfProxy::Init()
{
    SYSPREF_LOG(("GConfProxy:: Init GConfProxy\n"));
    if (!mSysPrefService)
        return PR_FALSE;
    if (mInitialized)
        return PR_TRUE;

    nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);
    if (!pref)
        return PR_FALSE;

    nsXPIDLCString gconfLibName;
    nsresult rv;

    //check if gconf-2 library is given in prefs
    rv = pref->GetCharPref(sPrefGConfKey, getter_Copies(gconfLibName));
    if (NS_SUCCEEDED(rv)) {
        //use the library name in the preference
        SYSPREF_LOG(("GConf library in prefs is %s\n", gconfLibName.get()));
        mGConfLib = PR_LoadLibrary(gconfLibName.get());
    }
    else {
        SYSPREF_LOG(("GConf library not specified in prefs, try the default: "
                     "%s and %s\n", sDefaultLibName1, sDefaultLibName2));
        mGConfLib = PR_LoadLibrary(sDefaultLibName1);
        if (!mGConfLib)
            mGConfLib = PR_LoadLibrary(sDefaultLibName2);
    }

    if (!mGConfLib) {
        SYSPREF_LOG(("Fail to load GConf library\n"));
        return PR_FALSE;
    }

    //check every func we need in the gconf library
    GConfFuncListType *funcList;
    PRFuncPtr func;
    for (funcList = sGConfFuncList; funcList->FuncName; ++funcList) {
        func = PR_FindFunctionSymbol(mGConfLib, funcList->FuncName);
        if (!func) {
            SYSPREF_LOG(("Check GConf Func Error: %s", funcList->FuncName));
            goto init_failed_unload;
        }
        funcList->FuncPtr = func;
    }

    InitFuncPtrs();

    mGConfClient = GConfClientGetDefault();

    // Don't unload past this point, since GConf's initialization of ORBit
    // causes atexit handlers to be registered.

    if (!mGConfClient) {
        SYSPREF_LOG(("Fail to Get default gconf client\n"));
        goto init_failed;
    }
    mInitialized = PR_TRUE;
    return PR_TRUE;

 init_failed_unload:
    PR_UnloadLibrary(mGConfLib);
 init_failed:
    mGConfLib = nsnull;
    return PR_FALSE;
}

nsresult
GConfProxy::GetBoolPref(const char *aMozKey, PRBool *retval)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);
    *retval = GConfClientGetBool(mGConfClient, MozKey2GConfKey(aMozKey), NULL);
    return NS_OK;
}

nsresult
GConfProxy::GetCharPref(const char *aMozKey, char **retval)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);

    gchar *str = GConfClientGetString(mGConfClient,
                                      MozKey2GConfKey(aMozKey), NULL);
    if (str) {
        *retval = PL_strdup(str);
        g_free(str);
    }
    return NS_OK;
}

nsresult
GConfProxy::GetIntPref(const char *aMozKey, PRInt32 *retval)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);
    *retval = GConfClientGetInt(mGConfClient, MozKey2GConfKey(aMozKey), NULL);
    return NS_OK;
}

nsresult
GConfProxy::NotifyAdd (PRUint32 aAtom, void *aUserData)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);

    const char *gconfKey = GetGConfKey(aAtom);
    if (!gconfKey)
        return NS_ERROR_FAILURE;

    if (!mObservers) {
        mObservers = new nsAutoVoidArray();
        if (mObservers == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
 
    GConfCallbackData *pData = (GConfCallbackData *)
        nsMemory::Alloc(sizeof(GConfCallbackData));
    NS_ENSURE_TRUE(pData, NS_ERROR_OUT_OF_MEMORY);

    pData->proxy = this;
    pData->userData = aUserData;
    pData->atom = aAtom;
    mObservers->AppendElement(pData);

    GConfClientAddDir(mGConfClient, gconfKey,
                      0, // GCONF_CLIENT_PRELOAD_NONE,  don't preload anything 
                      NULL);

    pData->notifyId = GConfClientNotifyAdd(mGConfClient, gconfKey,
                                           gconf_key_listener, pData,
                                           NULL, NULL);
    return NS_OK;
}

nsresult
GConfProxy::NotifyRemove (PRUint32 aAtom, const void *aUserData)
{
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_FAILURE);

    PRIntn count = mObservers->Count();
    if (count <= 0)
        return NS_OK;

    PRIntn i;
    GConfCallbackData *pData;
    for (i = 0; i < count; ++i) {
        pData = (GConfCallbackData *)mObservers->ElementAt(i);
        if (pData && pData->atom == aAtom && pData->userData == aUserData) {
            GConfClientNotifyRemove(mGConfClient, pData->notifyId);
            GConfClientRemoveDir(mGConfClient,
                                 GetGConfKey(pData->atom), NULL);
            mObservers->RemoveElementAt(i);
            nsMemory::Free(pData);
            break;
        }
    }
    return NS_OK;
}

void
GConfProxy::InitFuncPtrs()
{
    //gconf client funcs
    GConfClientGetDefault =
        (GConfClientGetDefaultType) sGConfFuncList[0].FuncPtr;
    GConfClientGetBool =
        (GConfClientGetBoolType) sGConfFuncList[1].FuncPtr;
    GConfClientGetString =
        (GConfClientGetStringType) sGConfFuncList[2].FuncPtr;
    GConfClientGetInt =
        (GConfClientGetIntType) sGConfFuncList[3].FuncPtr;
    GConfClientNotifyAdd =
        (GConfClientNotifyAddType) sGConfFuncList[4].FuncPtr;
    GConfClientNotifyRemove =
        (GConfClientNotifyRemoveType) sGConfFuncList[5].FuncPtr;
    GConfClientAddDir =
        (GConfClientAddDirType) sGConfFuncList[6].FuncPtr;
    GConfClientRemoveDir =
        (GConfClientRemoveDirType) sGConfFuncList[7].FuncPtr;

    //gconf entry funcs
    GConfEntryGetValue = (GConfEntryGetValueType) sGConfFuncList[8].FuncPtr;
    GConfEntryGetKey = (GConfEntryGetKeyType) sGConfFuncList[9].FuncPtr;

    //gconf value funcs
    GConfValueGetBool = (GConfValueGetBoolType) sGConfFuncList[10].FuncPtr;
    GConfValueGetString = (GConfValueGetStringType) sGConfFuncList[11].FuncPtr;
    GConfValueGetInt = (GConfValueGetIntType) sGConfFuncList[12].FuncPtr;
}

void
GConfProxy::OnNotify(void *aClient, void * aEntry, PRUint32 aNotifyId,
                     GConfCallbackData *aData)
{
    if (!mInitialized || !aEntry || (mGConfClient != aClient) || !aData)
        return;

    if (GConfEntryGetValue(aEntry) == NULL)
        return;

    PRUint32 prefAtom;
    nsresult rv = GetAtomForGConfKey(GConfEntryGetKey(aEntry), &prefAtom);
    if (NS_FAILED(rv))
        return;

    mSysPrefService->OnPrefChange(prefAtom, aData->userData);
}

nsresult
GConfProxy::GetAtom(const char *aKey, PRUint8 aNameType, PRUint32 *aAtom)
{
    if (!aKey)
        return NS_ERROR_FAILURE;
    PRUint32 prefSize = sizeof(sPrefNameMapping) / sizeof(sPrefNameMapping[0]);
    for (PRUint32 index = 0; index < prefSize; ++index) {
        if (!strcmp((aNameType == 0) ? sPrefNameMapping[index].mozPrefName :
                    sPrefNameMapping[index].gconfPrefName, aKey)) {
            *aAtom = index;
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

const char *
GConfProxy::GetKey(PRUint32 aAtom, PRUint8 aNameType)
{
    PRUint32 mapSize = sizeof(sPrefNameMapping) / sizeof(sPrefNameMapping[0]);
    if (aAtom >= 0 && aAtom < mapSize)
        return (aNameType == 0) ? sPrefNameMapping[aAtom].mozPrefName :
            sPrefNameMapping[aAtom].gconfPrefName;
    return NULL;
}

inline const char *
GConfProxy::MozKey2GConfKey(const char *aMozKey)
{
    PRUint32 atom;
    nsresult rv = GetAtomForMozKey(aMozKey, &atom);
    if (NS_SUCCEEDED(rv))
        return GetGConfKey(atom);
    return NULL;
}

/* static */
void gconf_key_listener (void* client, guint cnxn_id,
                         void *entry, gpointer user_data)
{
    SYSPREF_LOG(("...SYSPREF_LOG...key listener get called \n"));
    if (!user_data)
        return;
    GConfCallbackData *pData = NS_REINTERPRET_CAST(GConfCallbackData *,
                                                   user_data);
    pData->proxy->OnNotify(client, entry, cnxn_id, pData);
}
