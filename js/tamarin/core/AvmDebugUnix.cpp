/*
 *     AvmDebugUnix.cpp
 *
 * Copyright (C) 2005 Macromedia, Inc.  All rights reserved.
 *
 * This source file is part of the Macromedia Garbage Collector (avmplus)
 * 
 */

#include "avmplus.h"

#include <stdio.h>

/*************************************************************************/
/******************************* Debugging *******************************/
/*************************************************************************/

namespace avmplus
{
	void AvmDebugMsg(bool debuggerBreak, const char *format, ...)
	{
#ifdef _DEBUG
		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
#endif
	}
	
	void AvmDebugMsg(const char* msg, bool debugBreak)
	{
#ifdef _DEBUG
        fprintf( stderr, "%s", msg );
#endif
	}

	void AvmDebugMsg(const wchar* msg, bool debugBreak)
	{
#ifdef _DEBUG
        fprintf( stderr, "%s", msg );
#endif
	}
}
