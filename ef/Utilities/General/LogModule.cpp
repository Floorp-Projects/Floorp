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

// LogModule.cpp
//
// Scott M. Silver
//


#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include "prthread.h"
#include "prprf.h"
#include "LogModule.h"

#ifdef LOGMODULE_CLASS_IS_DEFINED

#define DEFAULT_LOG_FILENAME "ef.log"

// Default log file, if no module-specific files defined
static FILE* defaultLogFile = NULL;

LogModuleObject *LogModuleObject::allLogModules = NULL;

LogModuleObject::
LogModuleObject(const char* inModuleName, PRLogModuleOptions inOptions)
{
	name = strdup(inModuleName);
	level = PR_LOG_NONE;
	options = inOptions;
	file = NULL;

	// Add this module to the chain of all logging modules
	next = allLogModules;
	allLogModules = this;
}

LogModuleObject::
~LogModuleObject()
{
    if (name)
	free((void*)name);
    if (file)
	fclose(file);
}

FILE *LogModuleObject::
getFile() const
{
    if (file)
	return file;
    if (!defaultLogFile)
        defaultLogFile = fopen(DEFAULT_LOG_FILENAME, "w");
    return defaultLogFile;
}

size_t LogModuleObject::
logPrint(const char* inFormat, ...) const
{
	char *msg;
	size_t num_chars;
	PRThread *me;

	va_list ap;

	va_start(ap, inFormat);

	if (options & PR_LOG_OPTION_SHOW_THREAD_ID) {
		char *format;

		me = PR_GetCurrentThread();
		format = PR_smprintf("[%p]: %s", me, inFormat);
		if (!format)
			return 0;
		msg = PR_vsmprintf(format, ap);
		PR_smprintf_free(format);
	} else {
		msg = PR_vsmprintf(inFormat, ap);
	}

	if (!msg)
		return 0;

	num_chars = strlen(msg);

	FILE *f = getFile();
	PR_ASSERT(f);
	if (!f)
	    return 0;

	fputs(msg, f);
	if (options & PR_LOG_OPTION_ADD_NEWLINE)
		fputs("\n", f);
	
	PR_smprintf_free(msg);

	return num_chars;
}

bool LogModuleObject::
setLogFile(const char* inFileName)
{
    if (file)
	fclose(file);
    file = fopen(inFileName, "w");
    return (file != NULL);
}

void LogModuleObject::
flushLogFile()
{
    FILE *f;
    f = getFile();
    if (f)
	fflush(file);
}
#endif



