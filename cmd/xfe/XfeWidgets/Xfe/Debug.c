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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/Debug.c>											*/
/* Description:	Xfe widgets functions for debugging.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#ifdef DEBUG									/* ifdef DEBUG			*/

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

#include <Xfe/XfeP.h>

#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>

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
XfeDebugXmStringToStaticPSZ(XmString xmstr)
{
	String		result = DebugGetBuffer();
	String		psz_string;

	result[0] = '\0';

    assert( xmstr != NULL );

	if (xmstr != NULL)
	{
		psz_string = XfeXmStringGetPSZ(xmstr,XmFONTLIST_DEFAULT_TAG);

		if (psz_string != NULL)
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
XfeDebugGetStaticWidgetString(Widget w,String name)
{
	String		str = NULL;
	XmString	xmstr = (XmString) XfeGetValue(w,name);

    assert( xmstr != NULL );

	if (xmstr != NULL)
	{
		str = XfeDebugXmStringToStaticPSZ(xmstr);
		
		XmStringFree(xmstr);
	}

	return str;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* The following XfeDebugWidgets*() API allows for a list of 'debug'	*/
/* widget to be maintained.  Extra debugging info can then be printed	*/
/* for such widgets.													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Boolean
XfeDebugIsEnabled(Widget w)
{
	Boolean result = False;

	assert( XfeIsManager(w) || XfeIsPrimitive(w) );
	
	if (XfeIsPrimitive(w))
	{
		result = _XfeDebugTrace(w);
	}
	else
	{
		result = _XfemDebugTrace(w);
	}

	return result;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDebugPrintf(Widget w,char * format, ...)
{
	if (XfeDebugIsEnabled(w))
	{
		va_list arglist;

		va_start(arglist,format);
		
		vprintf(format,arglist);
		
		va_end(arglist);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeDebugPrintfFunction(Widget w,char * func_name,char * format, ...)
{
	if (XfeDebugIsEnabled(w))
	{
		assert( func_name != NULL );

		printf("%s(%s",func_name,XtName(w));

		if (format != NULL)
		{
			va_list arglist;
			
			va_start(arglist,format);
			
			vprintf(format,arglist);
			
			va_end(arglist);
		}

		printf(")\n");
	}
}
/*----------------------------------------------------------------------*/

#endif											/* endif DEBUG			*/
