/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*   ybtime.c --- yb functions dealing with front-end
                  timers and timeouts.
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "fe_proto.h"

/*
** FE_SetTimeout - Do whatever needs to be done to register a timeout to happen
** after msecs milliseconds.
**
** This function should return some unique ID for the timeout, or NULL
** if some operation fails.
**
** once the timeout has fired, it should not be fired again until
** re-registered.  That is, if the FE maintains a list of timeouts, it
** should remove the timeout after it's fired.  
*/
void*
FE_SetTimeout(TimeoutCallbackFunction func,
	      void *closure,
	      uint32 msecs)
{
}

/*
** FE_ClearTimeout - Do whatever needs to happen to unregister a
** timeout, given it's ID.  
*/
void 
FE_ClearTimeout(void *timer_id)
{
}
