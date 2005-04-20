
#include "nsIFactory.h"
#include "nsIPromptService.h"


#define NS_PROMPTSERVICE_CID \
 {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}

nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory);
