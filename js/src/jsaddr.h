/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996 Netscape Communications Corporation, All Rights Reserved.
 */

#ifndef jsaddr_h___
#define jsaddr_h___

JS_EXTERN_API(JSPropertyOp)
js_GetArgumentAddress();

JS_EXTERN_API(JSPropertyOp)
js_SetArgumentAddress();

JS_EXTERN_API(JSPropertyOp)
js_GetLocalVariableAddress();

JS_EXTERN_API(JSPropertyOp)
js_SetLocalVariableAddress();

#endif /* jsaddr_h___ */
