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

#ifndef PWCACAPI_H
#define PWCACAPI_H

/* contains a null terminated array of name and value stings 
 *
 * end index of name should always be equal to the end index of value
 *
 */

typedef struct _PCNameValuePair PCNameValuePair;
typedef struct _PCNameValueArray PCNameValueArray;

typedef void PCDataInterpretFunc (
				char *module,
				char *key,
				char *data, int32 data_size,
				char *type_buffer, int32 type_buffer_size, 
				char *url_buffer, int32 url_buffer_size,
				char *username_buffer, int32 username_buffer_size,
				char *password_buffer, int32 password_buffer_size);

/* returns 0 on success -1 on error 
 */
extern int PC_RegisterDataInterpretFunc(char *module, 
										PCDataInterpretFunc *func);

extern int PC_PromptUsernameAndPassword(MWContext *context,
										 char *prompt,
										 char **username,
										 char **password,
										 XP_Bool *remember_password,
										 XP_Bool  is_secure);

extern char *PC_PromptPassword(MWContext *context,
								 char *prompt,
								 XP_Bool *remember_password,
								 XP_Bool  is_secure);

extern char *PC_Prompt(MWContext *context,
                       char *prompt,
					   char *deft,
                       XP_Bool *remember,
					   XP_Bool  is_secure);

void PC_FreeNameValueArray(PCNameValueArray *array);

PCNameValueArray * PC_NewNameValueArray();

uint32 PC_ArraySize(PCNameValueArray *array);

char * PC_FindInNameValueArray(PCNameValueArray *array, char *name);

int PC_DeleteNameFromNameValueArray(PCNameValueArray *array, char *name);

void PC_EnumerateNameValueArray(PCNameValueArray *array, char **name, char **value, XP_Bool beginning);

int PC_AddToNameValueArray(PCNameValueArray *array, char *name, char *value);

void PC_CheckForStoredPasswordData(char *module, char *key, char **data, int32 *len);

int PC_DeleteStoredPassword(char *module, char *key);

PCNameValueArray * PC_CheckForStoredPasswordArray(char *module, char *key);

int PC_StoreSerializedPassword(char *module, char *key, char *data, int32 len);

int PC_StorePasswordNameValueArray(char *module, char *key, PCNameValueArray *array);

void PC_SerializeNameValueArray(PCNameValueArray *array, char **data, int32 *len);

PCNameValueArray * PC_CharToNameValueArray(char *data, int32 len);

/*, returns status
 */
int	PC_DisplayPasswordCacheAsHTML(URL_Struct *URL_s, 
							   FO_Present_Types format_out,
							   MWContext *context);

void PC_Shutdown();

#endif /* PWCACAPI_H */
