/* $Id: plugin.cpp,v 1.1 1998/09/25 18:01:37 ramiro%netscape.com Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "nppg.h"

static void useArgs( const char *fn, ... )
{	
    if (0&&fn)
	printf( "%s\n", fn );
}


extern "C"
void
FE_RegisterPlugins()
{
    useArgs("FE_RegisterPlugins");
}

extern "C"
NPPluginFuncs*
FE_LoadPlugin(void *plugin,
	      NPNetscapeFuncs *funcs,
              struct _np_handle* handle)
{
    useArgs("FE_LoadPlugin", plugin, funcs, handle);
    return 0;
}

extern "C"
void
FE_UnloadPlugin(void *plugin, struct _np_handle* handle)
{
    useArgs("FE_UnloadPlugin", plugin, handle);
}

extern "C"
void
FE_EmbedURLExit(URL_Struct *urls,
		int status,
		MWContext *cx)
{
    useArgs("FE_EmbedURLExit", urls, status, cx);
}

extern "C"
void
FE_ShowScrollBars(MWContext *context,
		  XP_Bool show)
{
    // Not reallt plugin-specific, but that's the only time
    // netscape uses it.
    useArgs("FE_ShowScrollBars", context, show);
}

#if defined(XP_UNIX)
extern "C"
NPError
FE_PluginGetValue(void *pdesc,
		  NPNVariable variable,
		  void *r_value)
{
    useArgs("FE_PluginGetValue", pdesc, variable, r_value);
    return 0;
}
#endif

#if defined(XP_WIN)
extern "C"
NPError
FE_PluginGetValue(MWContext *pContext, NPEmbeddedApp *pApp, 
                  NPNVariable variable, void *pRetVal)
{
    useArgs("FE_PluginGetValue", pContext, pApp, variable, pRetVal);
    return 0;
}
#endif
