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

PR_IMPLEMENT(JSPropertyOp)
js_GetArgumentAddress()
{
	return ((void *)js_GetArgument);
}

PR_IMPLEMENT(JSPropertyOp)
js_SetArgumentAddress()
{
	return ((void *)js_SetArgument);
}

PR_IMPLEMENT(JSPropertyOp)
js_GetLocalVariableAddress()
{
	return ((void *)js_GetLocalVariable);
}

PR_IMPLEMENT(JSPropertyOp)
js_SetLocalVariableAddress()
{
	return ((void *)js_SetLocalVariable);
}
