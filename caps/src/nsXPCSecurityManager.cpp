#include "jsapi.h"
#include "nsIXPCSecurityManager.h"

class nsXPCSecurityManager : public nsIXPCSecurityManager {
public:
	NS_DECL_ISUPPORTS
	/* void CanCreateWrapper (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj); */
    NS_IMETHOD CanCreateWrapper(JSContext * aJSContext,
                                const nsIID & aIID,
                                nsISupports *aObj);
    /* void CanCreateInstance (in JSContext aJSContext, in nsCIDRef aCID); */
    NS_IMETHOD CanCreateInstance(JSContext * aJSContext,
                                const nsCID & aCID);
    /* void CanGetService (in JSContext aJSContext, in nsCIDRef aCID); */
    NS_IMETHOD CanGetService(JSContext * aJSContext,
                                const nsCID & aCID);
    /* void CanCallMethod (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj,
                                in nsIInterfaceInfo aInterfaceInfo, in PRUint16 aMethodIndex, [const] in jsid aName); */
    NS_IMETHOD CanCallMethod(JSContext * aJSContext,
                                const nsIID & aIID,
                                nsISupports *aObj,
                                nsIInterfaceInfo *aInterfaceInfo,
                                PRUint16 aMethodIndex,
                                const jsid aName);
    /* void CanGetProperty (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj,
                                in nsIInterfaceInfo aInterfaceInfo, in PRUint16 aMethodIndex, [const] in jsid aName); */
    NS_IMETHOD CanGetProperty(JSContext * aJSContext,
                                const nsIID & aIID,
                                nsISupports *aObj,
                                nsIInterfaceInfo *aInterfaceInfo,
                                PRUint16 aMethodIndex,
                                const jsid aName);
    /* void CanSetProperty (in JSContext aJSContext, in nsIIDRef aIID, in nsISupports aObj,
                                in nsIInterfaceInfo aInterfaceInfo, in PRUint16 aMethodIndex, [const] in jsid aName); */
    NS_IMETHOD CanSetProperty(JSContext * aJSContext,
                                const nsIID & aIID,
                                nsISupports *aObj,
                                nsIInterfaceInfo *aInterfaceInfo,
                                PRUint16 aMethodIndex, 
                                const jsid aName);
	nsXPCSecurityManager();
private:
};

static NS_DEFINE_IID(knsXPCSecurityManagerIID, NS_IXPCSECURITYMANAGER_IID);
NS_IMPL_ISUPPORTS(nsXPCSecurityManager, knsXPCSecurityManagerIID);

nsXPCSecurityManager::nsXPCSecurityManager()
{
	NS_INIT_REFCNT();
	NS_ADDREF_THIS();
}
 
NS_IMETHODIMP
nsXPCSecurityManager::CanCreateWrapper(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj)
{
	return NS_OK;
}
 
NS_IMETHODIMP
nsXPCSecurityManager::CanCreateInstance(JSContext * aJSContext, const nsCID & aCID)
{
	return NS_OK;
}
 
NS_IMETHODIMP
nsXPCSecurityManager::CanGetService(JSContext * aJSContext, const nsCID & aCID)
{
	return NS_OK;
}
 
NS_IMETHODIMP
nsXPCSecurityManager::CanCallMethod(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName)
{
	return NS_OK;
}
 
NS_IMETHODIMP
nsXPCSecurityManager::CanGetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName)
{
	return NS_OK;
}
 
NS_IMETHODIMP
nsXPCSecurityManager::CanSetProperty(JSContext * aJSContext, const nsIID & aIID, nsISupports *aObj, nsIInterfaceInfo *aInterfaceInfo, PRUint16 aMethodIndex, const jsid aName)
{
	return NS_OK;
}