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

/* backend glue code to do password caching
 * Accepts password queries and does lookups in a secure database
 *
 * Written by Lou Montulli Jan 98
 */
#include "xp.h"
#include "prlog.h"
#include "mcom_db.h"
#include "pwcacapi.h"

#ifdef XP_MAC
#include <errno.h>
#endif

DB *pw_database=NULL;
XP_List *pc_interpret_funcs=NULL;

struct _PCNameValuePair {
        char *name;
        char *value;
};

struct _PCNameValueArray {

        int32           cur_ptr;
        int32           size;
        int32           first_empty;
        PCNameValuePair *pairs;
};

typedef struct {
	char				*module;
	PCDataInterpretFunc *func;
} PCInterpretModuleAssoc;

PUBLIC void
PC_Shutdown()
{

	if(pw_database)
	{
		(*pw_database->close)(pw_database);

		pw_database = NULL;
	}
}

PRIVATE int
pc_open_database(void)
{
	char* filename = "passcac.db";
	static Bool have_tried_open=FALSE;

    if(!pw_database)
      {
	HASHINFO hash_info = {
		4 * 1024,
		0,
		0,
#ifdef WIN16
		60 * 1024,
#else
		96 * 1024,
#endif
		0,
		0};

		/* @@@ add .db to the end of the files
		 */
		pw_database = dbopen(filename,
								O_RDWR | O_CREAT,
								0600,
								DB_HASH,
								&hash_info);

        if(!have_tried_open && !pw_database)
          {
    		XP_StatStruct stat_entry;

			have_tried_open = TRUE; /* only do this once */

            PR_LogPrint("Could not open cache database -- errno: %d",errno);

			/* if the file is zero length remove it
			 */
         	if(XP_Stat("", &stat_entry, xpCacheFAT) != -1)
			  {
           		if(stat_entry.st_size <= 0)
				  {
					XP_FileRemove("", xpCacheFAT);
				  }
			  }

			/* try it again */
			if (filename) {
				pw_database = dbopen(filename,
										O_RDWR | O_CREAT,
										0600,
										DB_HASH,
										0);
			}
			else 
				pw_database = NULL;
          }
	}

	/* return non-zero if the pw_database pointer is
	 * non-zero
	 */
	return((int) pw_database);

}

PRIVATE char * 
pc_gen_key(char *module, char *module_key)
{
	char *combo=NULL;

	StrAllocCopy(combo, module);
	StrAllocCat(combo, "\t");
	StrAllocCat(combo, module_key);

	return combo;
}

PRIVATE void
pc_separate_key(char *key, char **module, char **module_key)
{
	char *tab;

	*module = NULL;
	*module_key = NULL;

	if(!key)
		return;

	tab = XP_STRCHR(key, '\t');

	if(!tab)
		return;

	*tab = '\0';

	*module = XP_STRDUP(key);
	*module_key = XP_STRDUP(tab+1);

	*tab = '\t';
}

PRIVATE PCInterpretModuleAssoc *
pc_find_interpret_func(char *module)
{
	XP_List *list_ptr = pc_interpret_funcs;
	PCInterpretModuleAssoc *assoc;

	if(!module)
		return NULL;

	while((assoc = (PCInterpretModuleAssoc *) XP_ListNextObject(list_ptr)) != NULL)
	{
		if(!XP_STRCMP(module, assoc->module))
			return assoc;
	}

	return NULL;
}

/* returns 0 on success -1 on error */
PUBLIC int
PC_RegisterDataInterpretFunc(char *module, PCDataInterpretFunc *func)
{
    PCInterpretModuleAssoc *assoc;

	if(!pc_interpret_funcs)
	{
		pc_interpret_funcs = XP_ListNew();

		if(!pc_interpret_funcs)
			return -1;
	}

	if((assoc =	pc_find_interpret_func(module)) != NULL)
	{
		assoc->func = func;
		return 0;
	}

	assoc = XP_NEW(PCInterpretModuleAssoc);

	if(!assoc)
		return -1;

	assoc->module = XP_STRDUP(module);
	assoc->func = func;

	if(!assoc->module)
	{
		XP_FREE(assoc);
		return -1;
	}

	XP_ListAddObject(pc_interpret_funcs, assoc);

	return 0;
}

PRIVATE void
pc_lookup_module_info(char *key,
					  char *data, int32 data_size,
					  char *type_buffer, int type_buffer_size, 
					  char *url_buffer, int url_buffer_size,
					  char *username_buffer, int username_buffer_size,
					  char *password_buffer, int password_buffer_size)
{
	char *module, *module_key;
    PCInterpretModuleAssoc *assoc;

	*type_buffer = '\0';
	*url_buffer = '\0';
	*username_buffer = '\0';
	*password_buffer = '\0';

	pc_separate_key(key, &module, &module_key);

	if(!module || !module_key)
	{
		XP_FREEIF(module);
		XP_FREEIF(module_key);
		return;
	}

	/* lookup an explain function from the modules list and use it to interpret the data */
	/* @@@@ */
	if(0 == (assoc = pc_find_interpret_func(module)))
	{
		/* cant find one */
		return;
	}

	(*assoc->func)(module,
					module_key,
					data, data_size,
					type_buffer, type_buffer_size, 
					url_buffer, url_buffer_size,
					username_buffer, username_buffer_size,
					password_buffer, password_buffer_size);

}

/*, returns status
 */
PUBLIC int
PC_DisplayPasswordCacheAsHTML(URL_Struct *URL_s, 
							   FO_Present_Types format_out,
							   MWContext *context)
{
	DBT key, data;
	int status = 1;
	NET_StreamClass *stream; 
	char tmp_buffer[512];
	char type_buffer[256];
	char url_buffer[512];
	char username_buffer[256];
	char password_buffer[256];

	format_out = CLEAR_CACHE_BIT(format_out);
	StrAllocCopy(URL_s->content_type, TEXT_HTML);
	stream = NET_StreamBuilder(format_out,
							   URL_s,
							   context);

	if(!stream)
	{
		return MK_UNABLE_TO_CONVERT;
	}


    /* define a macro to push a string up the stream
     * and handle errors
     */
#define PUT_PART(part)                                                  \
status = (*stream->put_block)(stream,                      \
                                        part ? part : "Unknown",        \
                                        part ? XP_STRLEN(part) : 7);    \
if(status < 0)                                                          \
  goto END;

    if(!pc_open_database())
    {
        XP_STRCPY(tmp_buffer, "The password database is currently unopenable");
        PUT_PART(tmp_buffer);
        goto END;
    }

    if(0 != (*pw_database->seq)(pw_database, &key, &data, R_FIRST))
    {
        XP_STRCPY(tmp_buffer, "The password database is currently empty");
        PUT_PART(tmp_buffer);
        goto END;
    }

	do {

		pc_lookup_module_info(key.data, 
							 data.data, data.size,
							 type_buffer, sizeof(type_buffer), 
							 url_buffer, sizeof(url_buffer),
							 username_buffer, sizeof(username_buffer),
							 password_buffer, sizeof(password_buffer));

		PUT_PART("Protocol: ");
		PUT_PART(type_buffer);

		PUT_PART("<br>\nURL: ");
		PUT_PART(url_buffer);

		PUT_PART("<br>\nUsername: ");
		PUT_PART(username_buffer);
		
		PUT_PART("<br>\nPassword: ");
		PUT_PART(password_buffer);

		PUT_PART("\n<HR>\n");

	} while (0 == (*pw_database->seq)(pw_database, &key, &data, R_NEXT));
    
END:
    if(status < 0)
        (*stream->abort)(stream, status);
    else
        (*stream->complete)(stream);

    return status;
}
		
PUBLIC int
PC_PromptUsernameAndPassword(MWContext *context,
							 char *prompt,
							 char **username,
							 char **password,
							 XP_Bool *remember,
							 XP_Bool is_secure)
{
	*remember = TRUE;
	*remember = FALSE;

	return FE_PromptUsernameAndPassword(context, prompt, username, password);
}

PUBLIC char *
PC_PromptPassword(MWContext *context,
							 char *prompt,
							 XP_Bool *remember,
							 XP_Bool is_secure)
{
	*remember = TRUE;
	*remember = FALSE;

	return FE_PromptPassword(context, prompt);
}

PUBLIC char *
PC_Prompt(MWContext *context,
		 char *prompt,
		 char *deflt,
		 XP_Bool *remember,
		 XP_Bool is_secure)
{
	*remember = TRUE;
	*remember = FALSE;

	return FE_Prompt(context, prompt, deflt);
}

PUBLIC void
PC_FreeNameValueArray(PCNameValueArray *array)
{
	int index;
    if(array)
    {
        if(array->pairs)
        {
            for(index=0; index < array->first_empty; index++)
			{
                XP_FREEIF(array->pairs[index].name);
                XP_FREEIF(array->pairs[index].value);
			}
            XP_FREE(array->pairs);
        }
        XP_FREE(array);
    }
}


#define MIN_ARRAY_SIZE 4
#define GROW_ARRAY_BY 4

PRIVATE PCNameValueArray *
pc_new_namevaluearray(int init_size)
{
	PCNameValueArray *array = XP_NEW_ZAP(PCNameValueArray);

	if(!array)
		return NULL;

	array->pairs = (PCNameValuePair*)XP_CALLOC(init_size, sizeof(PCNameValuePair));
	array->size = init_size;
	array->first_empty = 0;
	array->cur_ptr = 0;

	if(!array->pairs)
	{
		PC_FreeNameValueArray(array);
		return NULL;
	}

	return array;
}

PUBLIC PCNameValueArray *
PC_NewNameValueArray()
{
	return pc_new_namevaluearray(MIN_ARRAY_SIZE);
}

PUBLIC uint32
PC_ArraySize(PCNameValueArray *array)
{
	return(array->first_empty);
}

/* returns value for a given name
 */
char *
PC_FindInNameValueArray(PCNameValueArray *array, char *name)
{
	int i;
	for(i=0; i<array->first_empty; i++)
	{
		if(!XP_STRCMP(array->pairs[i].name, name))
			return XP_STRDUP(array->pairs[i].value);
	}
	
	return NULL;
	
}

PUBLIC int
PC_DeleteNameFromNameValueArray(PCNameValueArray *array, char *name)
{
	int i;

	if(!array)
		return -1;

	for(i=0; i<array->first_empty; i++)
	{
		if(!XP_STRCMP(array->pairs[i].name, name))
		{
			/* found it */

			/* delete it */
			XP_FREE(array->pairs[i].name);
			XP_FREEIF(array->pairs[i].value);

			/* move everything */
			array->first_empty--;

			if(array->first_empty > i+1)
				XP_MEMCPY(&array->pairs[i], &array->pairs[i+1], (array->first_empty - (i+1)) * sizeof(PCNameValuePair));

			return 0;
		}
	}

	return -1;
}

/* enumerates the array.  DO NOT free the name and value results 
 *
 * set beggining to TRUE to start over at the beginning
 */
PUBLIC void
PC_EnumerateNameValueArray(PCNameValueArray *array, char **name, char **value, XP_Bool beginning)
{

	*name = NULL;
	*value = NULL;
	
	if(!array)
		return;

	if(beginning)
	{
		array->cur_ptr = 0;
	}
	else if(array->cur_ptr >= array->first_empty)
	{
		return;
	}

	*name = array->pairs[array->cur_ptr].name;
	*value = array->pairs[array->cur_ptr].value;

	array->cur_ptr++;
		
	return;
}

/* private ver. takes pre malloced name and value strings */
PRIVATE int
pc_add_to_namevaluearray(PCNameValueArray *array, char *name, char *value)
{
	if(array)
	{
		if(array->first_empty >= array->size-1)
		{
			/* need to grow */

			array->size += GROW_ARRAY_BY;
			array->pairs = (PCNameValuePair *)XP_REALLOC(array->pairs, array->size * sizeof(PCNameValuePair));
		}

		if(!array->pairs)
		{
			array->size = 0;
			return -1;
		}
		
		array->pairs[array->first_empty].name = name;
		array->pairs[array->first_empty].value = value;
		array->first_empty++;

		if(!array->pairs[array->first_empty-1].name
		   || !array->pairs[array->first_empty-1].value)
			return -1;

		return 0;
	}

	return -1;
}

/* adds to end of name value array 
 *
 * Possible to add duplicate names with this
 */
PUBLIC int
PC_AddToNameValueArray(PCNameValueArray *array, char *name, char *value)
{
	char *m_name = XP_STRDUP(name);
	char *m_value = XP_STRDUP(value);

	if(!m_name || !m_value)
	{
		XP_FREEIF(m_name);
		XP_FREEIF(m_value);
		return -1;
	}

	return pc_add_to_namevaluearray(array, name, value);
}

/* takes a key string as input and returns a char * pointer
 * in data to the serialized data structure previously stored or NULL.
 * len will be filled in to the length of the data string
 *
 * A module name is also passed in to guarentee that a key from
 * another module is never returned by an accidental key match.
 */
PUBLIC void
PC_CheckForStoredPasswordData(char *module, char *key, char **data, int32 *len)
{
	DBT k_key, k_data;
	char *combo;
	int status;

	*len = 0;
	*data = NULL;

	if(!pc_open_database())
		return;

	if((combo = pc_gen_key(module, key)) == NULL)
		return;

	k_key.size = XP_STRLEN(combo);
	k_key.data = combo;

	status = (*pw_database->get)(pw_database, &k_key, &k_data, 0);
		
	XP_FREE(combo);

	if(status != 0)
		return;

	*data = k_data.data;
	*len = k_data.size;

	return;
}

/* returns 0 on success else -1 
 */
PUBLIC int
PC_DeleteStoredPassword(char *module, char *key)
{
	DBT k_key;
	char *combo;
	int status;

	if(!pc_open_database())
		return -1;

	if((combo = pc_gen_key(module, key)) == NULL)
		return -1;

	k_key.size = XP_STRLEN(combo);
	k_key.data = combo;

	status = (*pw_database->del)(pw_database, &k_key, 0);

	XP_FREE(combo);

	if(status != 0)
		return -1;

	return 0;
}

/* takes a key string as input and returns a name value array
 *
 * A module name is also passed in to guarentee that a key from
 * another module is never returned by an accidental key match.
 */
PUBLIC PCNameValueArray *
PC_CheckForStoredPasswordArray(char *module, char *key)
{
	char *data;
	int32 len;

	PC_CheckForStoredPasswordData(module, key, &data, &len);

	if(!data)
		return NULL;

	return PC_CharToNameValueArray(data, len);
}

/* stores a serialized data stream in the password database
 * returns 0 on success
 */
PUBLIC int
PC_StoreSerializedPassword(char *module, char *key, char *data, int32 len)
{
	char *combo;
	DBT k_key, k_data;
	int status;

	if(!pc_open_database())
		return 0;

	if((combo = pc_gen_key(module, key)) == NULL)
		return -1;

	k_key.size = XP_STRLEN(combo)+1;
	k_key.data = combo;

	k_data.data = data;
	k_data.size = len;

	status = (*pw_database->put)(pw_database, &k_key, &k_data, 0);
		
	XP_FREE(combo);

	if(status != 0)
		return -1;

	status = (*pw_database->sync)(pw_database, 0);

	return 0;
}

/* stores a name value array in the password database
 * returns 0 on success
 */
PUBLIC int
PC_StorePasswordNameValueArray(char *module, char *key, PCNameValueArray *array)
{
	char *data;
	int32 len;
	int status;

	PC_SerializeNameValueArray(array, &data, &len);

	if(!data)
		return -1;

	status = PC_StoreSerializedPassword(module, key, data, len);

	XP_FREE(data);

	return status;
}

#define SERIALIZER_VERSION_NUM 1

/* takes a name value array and serializes to a char string.
 * string is returned in "data" with length "len"
 *
 * data will always be NULL on error
 */
PUBLIC void
PC_SerializeNameValueArray(PCNameValueArray *array, char **data, int32 *len)
{
	int32 total_size, net_long;
	char *cur_ptr;
	char *name, *value;

	*len = 0;
	*data = NULL;

	XP_ASSERT(array && array->pairs);

	if(!array)
		return;

	total_size = sizeof(int32)*3; /* start with checksum + ver + array_size amount */
	
	/* determine size of arrays */
	PC_EnumerateNameValueArray(array, &name, &value, TRUE);
	while(name)
	{
		total_size += sizeof(int32); /* size of name len */
		total_size += XP_STRLEN(name)+1;

		total_size += sizeof(int32); /* size of value len */
		if(value)
			total_size += XP_STRLEN(value)+1;

		PC_EnumerateNameValueArray(array, &name, &value, FALSE);
	}

	/* malloc enough space */
	*data = XP_ALLOC(sizeof(char) * total_size);
	
	if(!*data)
		return;
	
	cur_ptr = *data;

	net_long = PR_htonl(total_size);
  	XP_MEMCPY(cur_ptr, &net_long, sizeof(int32));
  	cur_ptr += sizeof(int32);
	
	net_long = PR_htonl(SERIALIZER_VERSION_NUM);
  	XP_MEMCPY(cur_ptr, &net_long, sizeof(int32));
  	cur_ptr += sizeof(int32);
	
	net_long = PR_htonl(PC_ArraySize(array));
  	XP_MEMCPY(cur_ptr, &net_long, sizeof(int32));
  	cur_ptr += sizeof(int32);
	
	PC_EnumerateNameValueArray(array, &name, &value, TRUE);
	while(name)
	{
  		net_long = PR_htonl((name ? XP_STRLEN(name)+1 : 0)); 
  		XP_MEMCPY(cur_ptr, &net_long, sizeof(int32));
  		cur_ptr += sizeof(int32);
		net_long = PR_ntohl(net_long);  /* convert back to true len */
  		if(net_long)
      		XP_MEMCPY((void *)cur_ptr, name, net_long);
  		cur_ptr += net_long;

  		net_long = PR_htonl((value ? XP_STRLEN(value)+1 : 0)); 
  		XP_MEMCPY(cur_ptr, &net_long, sizeof(int32));
  		cur_ptr += sizeof(int32);
		net_long = PR_ntohl(net_long);  /* convert back to true len */
  		if(net_long)
      		XP_MEMCPY((void *)cur_ptr, value, net_long);
  		cur_ptr += net_long;

		PC_EnumerateNameValueArray(array, &name, &value, FALSE);
	}
	
	*len = total_size;

	return;
}

/* returns a PCNameValueArray from serialized char data.
 *
 * returns NULL on error
 */
PUBLIC PCNameValueArray *
PC_CharToNameValueArray(char *data, int32 len)
{
	int32 host_long, str_len, len_read, array_size, index;
	char *cur_ptr;
	char *name, *value;
	PCNameValueArray *array;

	/* must be at least 12 bytes, len and ver and array_size */
	XP_ASSERT(len >= 12);
	
	if(len < 12)
		return NULL;

	cur_ptr = data;

	/* read first 4 bytes as checksum */
	XP_MEMCPY(&host_long, cur_ptr, 4);
	host_long = PR_ntohl(host_long);
	cur_ptr += sizeof(int32);
	len_read = sizeof(int32);
	
	if(host_long != len)
	{
		XP_ASSERT(0); /* this one might happen on db error */
		return NULL;  /* failed checksum */
	}

	/* read next 4 bytes as ver */
	XP_MEMCPY(&host_long, cur_ptr, 4);
	host_long = PR_ntohl(host_long);
	cur_ptr += sizeof(int32);
	len_read += sizeof(int32);
	
	if(host_long != SERIALIZER_VERSION_NUM)
	{
		XP_ASSERT(0);
		return NULL;  /* failed ver check */
	}

	/* read next 4 bytes as array_size */
	XP_MEMCPY(&array_size, cur_ptr, 4);
	array_size = PR_ntohl(array_size);
	cur_ptr += sizeof(int32);
	len_read += sizeof(int32);

	/* malloc arrays */
	array = pc_new_namevaluearray(array_size);
	if(!array)
		return NULL;

	index = 0;
	while(len_read < len)
	{
		/* next 4 bytes is length of name string */
		XP_MEMCPY(&str_len, cur_ptr, 4);
		str_len = PR_ntohl(str_len);
		cur_ptr += sizeof(int32);
		len_read += sizeof(int32);

		if(len_read + str_len > len)
			goto error_out;
	
		name = XP_ALLOC(str_len * sizeof(char));
		if(!name)
			goto error_out;

		XP_MEMCPY(name, cur_ptr, str_len);
		len_read += str_len;
		cur_ptr += str_len;

		if(len_read >= len)
			goto error_out;
		
		/* next 4 bytes is length of value string */
		XP_MEMCPY(&str_len, cur_ptr, 4);
		str_len = PR_ntohl(str_len);
		cur_ptr += sizeof(int32);
		len_read += sizeof(int32);

		if(len_read + str_len > len)
			goto error_out;
	
		value = XP_ALLOC(str_len * sizeof(char));
		if(!value)
			goto error_out;

		XP_MEMCPY(value, cur_ptr, str_len);
		len_read += str_len;
		cur_ptr += str_len;

		pc_add_to_namevaluearray(array, name, value);

		index++;
	}

	XP_ASSERT(len_read == len);

	return array;

error_out:

	XP_ASSERT(0);

	PC_FreeNameValueArray(array);

	return NULL;
}
