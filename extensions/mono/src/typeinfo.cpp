#include <nsXPCOM.h>
#include <nsDebug.h>
#include <nsIInterfaceInfoManager.h>
#include <nsCOMPtr.h>
#include <xptinfo.h>
#include <stdio.h>
#include <nsString.h>
#include <nsCRT.h>

nsIInterfaceInfoManager* infomgr;

extern "C" nsIEnumerator *
typeinfo_EnumerateInterfacesStart()
{
    nsIEnumerator *e;
    if (NS_FAILED(infomgr->EnumerateInterfaces(&e)) || NS_FAILED(e->First()))
        return 0;

    return e;
}

extern "C" char *
typeinfo_EnumerateInterfacesNext(nsIEnumerator *e)
{
    nsCOMPtr<nsISupports> isup;
    while (1) {
        if (e->IsDone() != NS_ENUMERATOR_FALSE ||
            NS_FAILED(e->CurrentItem(getter_AddRefs(isup))) || !isup) {
            break;
        }
            
        e->Next();
        nsCOMPtr<nsIInterfaceInfo> iface(do_QueryInterface(isup));
        if (!iface)
            break;
        
        PRBool scriptable;
        if (NS_SUCCEEDED(iface->IsScriptable(&scriptable)) &&
            !scriptable) {
            continue;
        }
        
        char *name;
        iface->GetName(&name);
        return name;
    }

    return 0;
}

extern "C" void
typeinfo_EnumerateInterfacesStop(nsIEnumerator *e)
{
    NS_IF_RELEASE(e);
}

extern "C" int
typeinfo_GetParentInfo(const char *ifaceName,
                       char **parentName,
                       PRUint16 *parentMethodCount)
{
    nsIInterfaceInfo *iinfo;
    nsresult res;

    res = infomgr->GetInfoForName(ifaceName, &iinfo);
    if (NS_FAILED(res))
        return res;

    nsCOMPtr<nsIInterfaceInfo> parent;
    res = iinfo->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(res))
        return res;

    if (!parent) {
        *parentName = 0;
        *parentMethodCount = 0;
        return NS_OK;
    }

    res = parent->GetName(parentName);
    if (NS_FAILED(res))
        return res;

    res = parent->GetMethodCount(parentMethodCount);
    return res;
}

extern "C" int
typeinfo_GetAllMethodData(const char *ifaceName,
                          const nsXPTMethodInfo ***infos,
                          PRUint16 *methodCount)
{
    nsIInterfaceInfo *iinfo;
    nsresult res;
    
    res = infomgr->GetInfoForName(ifaceName, &iinfo);
    if (NS_FAILED(res))
        return res;

    res = iinfo->GetMethodCount(methodCount);
    if (NS_FAILED(res))
        return res;

    const nsXPTMethodInfo **infoarr = 
        new const nsXPTMethodInfo*[*methodCount];

    *infos = &infoarr[0];

    for (int i = 0; i < *methodCount; i++) {
        res = iinfo->GetMethodInfo(i, &infoarr[i]);
        if (NS_FAILED(res)) {
            fprintf(stderr, "Getting method info for %s:%d: %08x\n",
                    ifaceName, i, res);
        }
    }

    return NS_OK;
}

extern "C" int
typeinfo_GetMethodData(const char *ifaceName, int methIndex,
                       const nsXPTMethodInfo **info)
{
    nsIInterfaceInfo *iinfo;
    nsresult res;
    *info = 0;

    res = infomgr->GetInfoForName(ifaceName, &iinfo);
    if (NS_FAILED(res))
        return res;

    return iinfo->GetMethodInfo(methIndex, info);
}

extern "C" int
typeinfo_GetMethodData_iid_index(const nsID *iid, int index,
                                 const nsXPTMethodInfo **info)
{
    nsIInterfaceInfo *iinfo;
    nsresult res;
    *info = 0;

    res = infomgr->GetInfoForIID(iid, &iinfo);
    if (NS_FAILED(res))
        return res;

    return iinfo->GetMethodInfo(index, info);
}

extern "C" int
typeinfo_GetMethodData_byname(const char *ifaceName, const char *methName,
                              PRUint16 *index, const nsXPTMethodInfo **info)
{
    nsIInterfaceInfo *iinfo;
    nsresult res;
    *info = 0;

    res = infomgr->GetInfoForName(ifaceName, &iinfo);
    if (NS_FAILED(res))
        return res;

    return iinfo->GetMethodInfoForName(methName, index, info);
}

extern "C" int
typeinfo_GetIIDForParam(const char *ifaceName, int methIndex,
                        int param, nsID *iid)
{
    nsIInterfaceInfo *iinfo;
    nsresult res;

    // fprintf(stderr, "getting IID for %s:%d:%d\n", ifaceName, methIndex,
    //         param);

    res = infomgr->GetInfoForName(ifaceName, &iinfo);
    if (NS_FAILED(res)) {
        fprintf(stderr, "%d: FAILED (%08x)\n", __LINE__, res);
        return res;
    }

    const nsXPTMethodInfo *meth;
    res = iinfo->GetMethodInfo(methIndex, &meth);
    if (NS_FAILED(res)) {
        fprintf(stderr, "%d: FAILED (%08x)\n", __LINE__, res);
        return res;
    }

    const nsXPTParamInfo& paramInfo = meth->GetParam(param);
    res = iinfo->GetIIDForParamNoAlloc(methIndex, &paramInfo, iid);

    char *idstr = iid->ToString();
    // fprintf(stderr, "outgoing iid is %s\n", idstr);
    nsCRT::free(idstr);
    if (NS_FAILED(res)) {
        fprintf(stderr, "%d: FAILED (%08x)\n", __LINE__, res);
        return res;
    }

    return res;
}

extern "C" int
typeinfo_GetNameForIID(nsID *iid, char **ifaceName)
{
  *ifaceName = 0;
  return infomgr->GetNameForIID(iid, ifaceName);
}

extern "C" nsAString *
typeinfo_WrapUnicode(const PRUnichar *chars, PRUint32 length)
{
    return new nsString(chars, length);
}

extern "C" void
typeinfo_FreeWrappedUnicode(nsAString *str)
{
    delete str;
}
