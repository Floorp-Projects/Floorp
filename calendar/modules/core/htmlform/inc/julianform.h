/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _JULIANFORM_H
#define _JULIANFORM_H

#include "jdefines.h"
#include "netcburl.h"
#include "fe_proto.h"

XP_BEGIN_PROTOS

typedef	struct Julian_Form_Callback_Struct
{
	/*
	** callbackurl should be set to NET_CallbackURLCreate(), it's
	** in netcburl.h. Can also be set to nil.
	*/
	char*	(*callbackurl)(NET_CallbackURLFunc func, void* closure);

	/*
	** callbackurlfree should be set to NET_CallbackURLFree(), it's
	** in netcburl.h. Can also be set to nil.
	*/
	int		(*callbackurlfree)(NET_CallbackURLFunc func, void* closure);

	/*
	** Should Link to NET_ParseURL()
	*/
	char*	(*ParseURL)(const char *url, int wanted);

	/*
	** Should Link to FE_MakeNewWindow()
	*/
	MWContext*	(*MakeNewWindow)(MWContext *old_context, URL_Struct *url, char *window_name, Chrome *chrome);

	/*
	** Should Link to NET_CreateURLStruct ();
	*/
	URL_Struct* (*CreateURLStruct) (const char *url, NET_ReloadMethod force_reload);

	/*
	** Should Link to PA_BeginParseMDL()
	*/
	NET_StreamClass* (*BeginParseMDL) (FO_Present_Types format_out, void *init_data, URL_Struct *anchor, MWContext *window_id);

	/*
	** Should Link to NET_SACopy()
	*/
	char* (*SACopy) (char **dest, const char *src);

    /*
    ** Should Link to NET_SendMessageUnattended().  Added by John Sun 4-22-98.
    */
    int (*SendMessageUnattended) (MWContext* context, char* to, char* subject, char* otherheaders, char* body);

    /*
    ** Should Link to FE_DestroyWindow.  Added by John Sun 4-22-98.
    */
    void (*DestroyWindow) (MWContext* context);

	/*
    ** Should Link to FE_RaiseWindow.
    */
    void (*RaiseWindow) (MWContext* context);

	/*
    ** Should Link to Current MWContext.
    */
    MWContext* my_context;

    /*
    ** Should Link to XP_GetString.
    */
    char* (*GetString) (int i);

	/*
	** Should Link to XP_FindSomeContext()
	*/
	MWContext*	(*FindSomeContext)();

	/*
	** Should Link to XP_FindNamedContextInList()
	*/
	MWContext*	(*FindNamedContextInList)(MWContext* context, char *name);

	/*
	** Should Link to PREF_CopyCharPref()
	*/
	int	(*CopyCharPref)(const char *pref, char ** return_buf);

	/*
	** Should Link to NET_UnEscape()
	*/
	char*		(*UnEscape)(char *str);

	/*
	** Should Link to NET_PlusToSpace()
	*/
	void		(*PlusToSpace)(char *str);

	/*
	** Should Link to PREF_SetCharPref()
	*/
	int	(*SetCharPref)(const char *pref, const char* buf);

	/*
	** Should Link to FE_PromptUsernameAndPassword()
	*/
	Bool (*PromptUsernameAndPassword)(MWContext* window_id, char* message, char** username, char** password);

    /*
    ** Should Link to LO_ProcessTag().
    */
    intn (*ProcessTag)(void *data_object, PA_Tag *tag, intn status);

#if defined(XP_WIN)||defined(XP_UNIX)
    /*
    ** Should link to FEU_GetJulianPath.  Get the path to the Julian directory.  Added by John Sun 5-14-98.
    */
    void (*GetJulianPath) (char ** julianPath, void * emptyArg);
#endif

	/*
	** Should Link to PREF_GetBoolPref()
	*/
	int	(*BoolPref)(const char *pref, XP_Bool * return_val);

	/*
	** Should Link to PREF_GetIntPref()
	*/
	int	(*IntPref)(const char *pref, int32 * return_int);

} Julian_Form_Callback_Struct, *pJulian_Form_Callback_Struct;

/*
** Caller disposes of callbacks.
*/
XP_Bool JULIAN_PUBLIC jf_Initialize(pJulian_Form_Callback_Struct callbacks);

void JULIAN_PUBLIC *jf_New(char *calendar_mime_data, XP_Bool bFoundNLSDataDirectory);
void JULIAN_PUBLIC jf_Destroy(void *instdata);
void JULIAN_PUBLIC jf_Shutdown(void);
char JULIAN_PUBLIC *jf_getForm(void *instdata);
void JULIAN_PUBLIC jf_setDetail(int detail_form);
void JULIAN_PUBLIC jf_callback(void *instdata, char* url, URL_Struct *URL_s);
void JULIAN_PUBLIC jf_detail_callback(void *instdata, char *url, URL_Struct *URL_s);

XP_END_PROTOS

#endif
