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

#include <xp_mcom.h>
#include "jdefines.h"
#include "julianform.h"
#include "nlsloc.h"

#ifdef XP_WIN32
/*#include "stdafx.h"*/
/*#include "feutil.h"*/
#endif

extern void* jform_CreateNewForm(char *, pJulian_Form_Callback_Struct, XP_Bool);
extern void  jform_DeleteForm(void *);
extern char* jform_GetForm(void *);
extern void  jform_CallBack(void *, void *);

pJulian_Form_Callback_Struct JulianForm_CallBacks;
XP_Bool bCallbacksSet = PR_FALSE ;

XP_Bool jf_Initialize(pJulian_Form_Callback_Struct callbacks)
{
    /* for some reason I have to do this little trick to get things to work */
    char * julian = NULL;
    char buf[1024];
    julian = &(buf[0]);

	JulianForm_CallBacks = XP_NEW_ZAP(Julian_Form_Callback_Struct);
    if (JulianForm_CallBacks && callbacks)
    {
		*JulianForm_CallBacks = *callbacks;
    }

#if defined(XP_WIN)||defined(XP_UNIX)
	if (JulianForm_CallBacks && callbacks && callbacks->GetJulianPath)
	{
        bCallbacksSet = PR_TRUE ;

        /* set the NLS locale30 path to the location of the locale30 from the registry */
        JulianForm_CallBacks->GetJulianPath(&julian, (void *)sizeof(buf));

        /* use path if it exist, otherwise you NS_NLS_DATADIRECTORY env var if it exists */
        /* if neither exists return FALSE */
        if (JXP_ACCESS(julian, 0) != -1)
        {
            NLS_Initialize(NULL, julian);
            return TRUE;
        }
        else 
        {
            char * nlsDDir = getenv("NS_NLS_DATADIRECTORY");
            if (nlsDDir == 0)
                return FALSE;
            else 
            {
                if (JXP_ACCESS(nlsDDir, 0) == -1)
                    return FALSE;
                else
                    return TRUE;
            }
        }
	}
#endif

    return FALSE;
}

void *jf_New(char *calendar_mime_data, XP_Bool bFoundNLSDataDirectory)
{
	return jform_CreateNewForm(calendar_mime_data, JulianForm_CallBacks, bFoundNLSDataDirectory);
}

void jf_Destroy(void *instdata)
{
    jform_DeleteForm(instdata);
}

void jf_Shutdown()
{
	XP_FREEIF(JulianForm_CallBacks);
}

char *jf_getForm(void *instdata)
{
	return jform_GetForm(instdata);
}

void jf_setDetail(int detail_form)
{
}

/* rhp - needed to add third parameter for other mime related calls */
void jf_callback(void *instdata, char *url, URL_Struct *URL_s)
{
	if (instdata && url)
	{
		jform_CallBack(instdata, url);
	}
}

/* rhp - needed to add third parameter for other mime related calls */
/* For some unknown reason there can only be one call back funtion/
** instdate per button
*/
void jf_detail_callback(void *instdata, char *url, URL_Struct *URL_s)
{
	if (instdata && url)
	{
		jform_CallBack(instdata, url);
	}
}
