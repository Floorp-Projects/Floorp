#include <rpcproxy.h>

#ifdef __cplusplus
extern "C"   {
#endif

EXTERN_PROXY_FILE( ISimpleDOMDocument )
EXTERN_PROXY_FILE( ISimpleDOMNode )
EXTERN_PROXY_FILE( ISimpleDOMText )

PROXYFILE_LIST_START
/* Start of list */
  REFERENCE_PROXY_FILE( ISimpleDOMDocument ),
  REFERENCE_PROXY_FILE( ISimpleDOMNode ),
  REFERENCE_PROXY_FILE( ISimpleDOMText ),
/* End of list */
PROXYFILE_LIST_END


DLLDATA_ROUTINES( aProxyFileList, GET_DLL_CLSID )

#ifdef __cplusplus
}  /*extern "C" */
#endif

