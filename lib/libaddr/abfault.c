/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* file: abfault.c 
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 02Dec1997 stubs
** <0> 22Oct1997 first draft
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/
/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* AB_Fault_kClassName /*i*/ = "AB_Fault";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


AB_API_IMPL(const char*)
AB_Fault_String(const AB_Fault* self) /*i*/
  /* Return a static string describing error e, provided the space is equal
   * to either AB_Fault_kErrnoSpace or AB_Fault_kGromitSpace,
   * and provided AB_CONFIG_KNOW_FAULT_STRINGS is
   * defined.  Otherwise returns a static string for "{unknown-fault-space}"
   * or for "{no-fault-strings}".
   */
{
	AB_USED_PARAMS_1(self);
    return "{no-fault-strings}";
}

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	AB_API_IMPL(char*)
	AB_Fault_AsXmlString(const AB_Fault* self, AB_Env* cev, /*i*/
		char* outXmlBuf)
	    /* <ab:fault string=\"%.32s\" c=\"#%08lX\" s =\"#%08lX/%.4s\"/> */
	{
		/* copied directly from public domain IronDoc: */
	    const char* format = 
	    "<ab:fault string=\"%.32s\" c=\"#%08lX\" s =\"#%08lX/%.4s\"/>";
	          
	    sprintf(outXmlBuf, format,
	      (char*) AB_Fault_String(self),  /* string="%.32s" */
	      
	      (long) self->sFault_Code,       /* c=\"%08lX\" */
	      
	      (long) self->sFault_Space,      /* s =\"#%08lX */
	      (char*) &self->sFault_Space     /* /%.4s\" */
	      );
		AB_USED_PARAMS_1(cev);
	    return outXmlBuf; /* return this parameter to simplify caller use */
	}
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#ifdef AB_CONFIG_DEBUG
	AB_API_IMPL(void)
	AB_Fault_Break(const AB_Fault* self, AB_Env* cev) /*i*/
		/* e.g. AB_Env_Break(ev, AB_Fault_AsXmlString(e, ev, buf)); */
	{
		char buf[ AB_Fault_kXmlBufSize + 2 ];
		AB_Env_Break(cev, AB_Fault_AsXmlString(self, cev, buf));
	}
#endif /*AB_CONFIG_DEBUG*/

#ifdef AB_CONFIG_TRACE
	AB_API_IMPL(void)
	AB_Fault_Trace(const AB_Fault* self, AB_Env* cev) /*i*/
		/* e.g. AB_Env_Trace(ev, AB_Fault_AsXmlString(e, ev, buf)); */
	{
		char buf[ AB_Fault_kXmlBufSize + 2 ];
		AB_Env_Break(cev, AB_Fault_AsXmlString(self, cev, buf));
	}
#endif /*AB_CONFIG_TRACE*/
