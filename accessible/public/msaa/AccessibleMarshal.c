#include <rpcproxy.h>

#ifdef __cplusplus
extern "C"   {
#endif

EXTERN_PROXY_FILE( ISimpleDOMDocument )
EXTERN_PROXY_FILE( ISimpleDOMNode )


PROXYFILE_LIST_START
/* Start of list */
  REFERENCE_PROXY_FILE( ISimpleDOMDocument ),
  REFERENCE_PROXY_FILE( ISimpleDOMNode ),
/* End of list */
PROXYFILE_LIST_END


DLLDATA_ROUTINES( aProxyFileList, GET_DLL_CLSID )

#ifdef __cplusplus
}  /*extern "C" */
#endif

