/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*   gnometime.c --- gnome functions dealing with front-end
                    timers and timeouts.
*/

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "fe_proto.h"

#include <gnome.h>

struct foo {
  void *real_closure;
  TimeoutCallbackFunction func;
  guint timer_id;
};

gint
timeout_trampoline(gpointer data)
{
  struct foo *blah = (struct foo*)data;

  (*blah->func)(blah->real_closure);

  free(blah);

  return FALSE;
}

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
  struct foo *blah = XP_NEW(struct foo);

  if (msecs == 0)
    {
      printf ("hmm. FE_SetTimeout == 0\n");
      msecs = 1;
    }

  blah->func = func;
  blah->real_closure = closure;
  blah->timer_id = gtk_timeout_add(msecs,
                                   timeout_trampoline,
                                   blah);

  return blah;
}

/*
** FE_ClearTimeout - Do whatever needs to happen to unregister a
** timeout, given it's ID.  
*/
void 
FE_ClearTimeout(void *timer_id)
{
  struct foo *blah = (struct foo*)timer_id;

  gtk_timeout_remove(blah->timer_id);

  free(blah);
}
