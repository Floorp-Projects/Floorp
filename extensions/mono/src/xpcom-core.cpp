#include <nsXPCOM.h>
#include <nsDebug.h>
#include <nsIInterfaceInfoManager.h>
#include <xptinfo.h>
#include <nsCOMPtr.h>

extern "C" int
StartXPCOM(nsIServiceManager** srvmgr)
{
    nsresult res = NS_InitXPCOM2(srvmgr, 0, 0);
    if (NS_SUCCEEDED(res)) {
        extern nsIInterfaceInfoManager *infomgr;
        infomgr = XPTI_GetInterfaceInfoManager();
    }
    return res;
}
