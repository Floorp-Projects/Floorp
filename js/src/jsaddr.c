/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996 Netscape Communications Corporation, All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

#include "jsapi.h"
#include "jsinterp.h"

/* These functions are needed to get the addresses of certain functions
 * in the JS module. On WIN32 especially, these symbols have a different
 * address from the actual address of these functions in the JS module.
 * This is because on WIN32, import function address fixups are done only
 * at load time and function calls are made by indirection - that is by
 * using a couple extra instructions to lookup the actual function address
 * in the importing module's import address table.
 */

JS_EXPORT_API(JSPropertyOp)
js_GetArgumentAddress()
{
	return ((void *)js_GetArgument);
}

JS_EXPORT_API(JSPropertyOp)
js_SetArgumentAddress()
{
	return ((void *)js_SetArgument);
}

JS_EXPORT_API(JSPropertyOp)
js_GetLocalVariableAddress()
{
	return ((void *)js_GetLocalVariable);
}

JS_EXPORT_API(JSPropertyOp)
js_SetLocalVariableAddress()
{
	return ((void *)js_SetLocalVariable);
}
