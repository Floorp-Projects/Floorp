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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/Debug.c>											*/
/* Description:	Xfe widgets functions for debugging.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifdef DEBUG									/* ifdef DEBUG			*/

#include <stdlib.h>
#include <ctype.h>

#include <Xfe/XfeP.h>
#include <Xm/RepType.h>

#define DEBUG_BUFFER_SIZE 2048

/*
 * This memory will leak, but only in debug builds.
 */
static String
DebugGetBuffer(void)
{
	static String _debug_buffer = NULL;

	if (!_debug_buffer)
	{
		_debug_buffer = (String) XtMalloc(sizeof(char) * DEBUG_BUFFER_SIZE);
	}

	assert( _debug_buffer != NULL );

	return _debug_buffer;
}

/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/* extern */ String
XfeDebugXmStringToStaticPSZ(XmString xmstring)
{
	String		result = DebugGetBuffer();
	String		psz_string;

	result[0] = '\0';

	if (xmstring)
	{
		psz_string = XfeXmStringGetPSZ(xmstring,XmFONTLIST_DEFAULT_TAG);

		if (psz_string)
		{
			strcpy(result,psz_string);

			XtFree(psz_string);
		}
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDebugPrintArgVector(FILE *		fp,
					   String		prefix,
					   String		suffix,
					   ArgList		av,
					   Cardinal		ac)
{
	Cardinal i;

	for(i = 0; i < ac; i++)
	{
		fprintf(fp,"%s: av[%d] = '%s'%s",
				prefix,
				i,
				av[i].name,
				suffix);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeDebugRepTypeValueToName(String rep_type,unsigned char value)
{
	String		result = DebugGetBuffer();
	Boolean		found = False;

	if (rep_type)
	{
		XmRepTypeId id = XmRepTypeGetId(rep_type);

		if (id != XmREP_TYPE_INVALID)
		{
			XmRepTypeEntry entry = XmRepTypeGetRecord(id);

			if (entry)
			{
				Cardinal i = 0;

				while ((i < entry->num_values) && !found)
				{
					if (entry->values[i] == value)
					{
						found = True;

						strcpy(result,entry->value_names[i]);
					}

					i++;
				}

 				XtFree((char *) entry);
			}
		}
	}
	
	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ unsigned char
XfeDebugRepTypeNameToValue(String rep_type,String name)
{
	unsigned char result = 99;
	Boolean found = False;

	if (rep_type)
	{
		XmRepTypeId id = XmRepTypeGetId(rep_type);

		if (id != XmREP_TYPE_INVALID)
		{
			XmRepTypeEntry entry = XmRepTypeGetRecord(id);

			if (entry)
			{
				Cardinal i = 0;

				while ((i < entry->num_values) && !found)
				{
					if (strcmp(entry->value_names[i],name) == 0)
					{
						result = entry->values[i];

						found = True;
					}

					i++;
				}

 				XtFree((char *) entry);
			}
		}
	}
	
	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ unsigned char
XfeDebugRepTypeIndexToValue(String rep_type,Cardinal i)
{
	unsigned char result = 99;

	if (rep_type)
	{
		XmRepTypeId id = XmRepTypeGetId(rep_type);

		if (id != XmREP_TYPE_INVALID)
		{
			XmRepTypeEntry entry = XmRepTypeGetRecord(id);

			if (entry)
			{
				result = entry->values[i];

 				XtFree((char *) entry);
			}
		}
	}
	
	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeDebugGetWidgetString(Widget w,String name)
{
	XmString xmstr = NULL;

	XtVaGetValues(w,name,&xmstr,NULL);

	if (xmstr)
	{
		return XfeDebugXmStringToStaticPSZ(xmstr);
	}

	return NULL;
}
/*----------------------------------------------------------------------*/

#endif											/* endif DEBUG			*/
