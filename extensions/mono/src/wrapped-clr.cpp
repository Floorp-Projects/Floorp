#include <nsIInterfaceInfoManager.h>
#include <xptcall.h>
#include <xpt_xdr.h>
#include <nsCRT.h>

#include <stdio.h>

class WrappedCLR : public nsXPTCStubBase
{
public:
    typedef int (*MethodInvoker)(int, nsXPTCMiniVariant *);

    NS_DECL_ISUPPORTS
    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo **aInfo);
    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo *info,
                          nsXPTCMiniVariant *params);
    WrappedCLR(MethodInvoker del, const nsIID &id) :
        mIID(id), mDelegate(del) { }
    virtual ~WrappedCLR() { }
private:
    nsIID mIID;
    MethodInvoker mDelegate;
};

NS_IMPL_ISUPPORTS1(WrappedCLR, nsISupports);

NS_IMETHODIMP
WrappedCLR::GetInterfaceInfo(nsIInterfaceInfo **info)
{
    extern nsIInterfaceInfoManager *infomgr;
    return infomgr->GetInfoForIID(&mIID, info);
}

NS_IMETHODIMP
WrappedCLR::CallMethod(PRUint16 methodIndex,
                       const nsXPTMethodInfo *info,
                       nsXPTCMiniVariant *params)
{
    fprintf(stderr, "Calling %s via %p\n", info->GetName(), mDelegate);
    return mDelegate(methodIndex, params);
}

extern "C" WrappedCLR *
WrapCLRObject(WrappedCLR::MethodInvoker del, const nsIID &id)
{
    char *idstr = id.ToString();
    fprintf(stderr, "Wrapping %p as %s\n", del, idstr);
    nsCRT::free(idstr);
    return new WrappedCLR(del, id);
}
