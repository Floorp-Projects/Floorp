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

// LogModule.h
//
// Scott M. Silver
//

// PRLogModuleInfo wrapper

// Usage:
//
// UT_DEFINE_MODULE(MyNewModuleName);		// notice no quotes
//
// void test()
// {
//		UT_LOG(MyNewModuleName, PR_LOG_ALWAYS, ("%s", "Hello World"));
// }
//
// This will do the appropriate logging to a module named "MyNewModuleName"
// as defined in "prlog.h".  See "prlog.h" for how to turn on this
// LogModule (as in make its output visible to the user).
//
// If you wish to share a particular module across file boundaries
// use the UT_LOG_MODULE macro in a ".cpp" file, and then use
// the UT_EXTERN_LOG_MODULE macro in a header (or a ".cpp" file) to
// share the module.
//
// Theoretically adding the appropriate decorator for dll exportation
// is possible too, but has not been tested.
//
// If you wish to pass around a LogModule, you need to
// make your function take a LogModuleObject as a parameter, and then
// use the UT_OBJECTLOG function instead of UT_LOG to log some data.
// For example:
//
// UT_DEFINE_MODULE(MyNewModuleName);
//
// void main()
// {
//		test2(UT_LOG_MODULE(MyNewModuleName);
// }
//
// void test2(LogModuleObject inModuleObject)
// {
//		UT_OBJECTLOG(inModuleObject, PR_LOG_ALWAYS, ("%s", "Hello World"));
// }
//
// All the UT* stuff in _this_ file disappears when PR_LOGGING is not defined
// The actual LogModule objects are created as global objects named 
// g__MyNewModuleName.
//
// UT = Utility

#ifndef _H_LOGMODULE
#define _H_LOGMODULE

#include "prlog.h"
#include "Exports.h"

#define PR_LOG_OPTION_NIL               0x0000
#define PR_LOG_OPTION_SHOW_THREAD_ID    0x0001
#define PR_LOG_OPTION_ADD_NEWLINE       0x0002
#define PR_LOG_OPTION_DEFAULT           (0)
typedef PRUint16 PRLogModuleOptions;

#ifdef PR_LOGGING

// Don't use this class directly, use the UT_* macros below
#define LOGMODULE_CLASS_IS_DEFINED

class NS_EXTERN LogModuleObject
{
	const char *		name;
    PRLogModuleLevel	level;
	PRLogModuleOptions	options;
    FILE *				file;
	LogModuleObject*	next;

	static LogModuleObject *allLogModules;

public:
				LogModuleObject(const char* inModuleName, 
					            PRLogModuleOptions inOptions = PR_LOG_OPTION_DEFAULT);
				~LogModuleObject();

	bool		setLogFile(const char* inFileName);
    void        flushLogFile();
	size_t		logPrint(const char* inFormat, ...) const;
    void        setLogLevel(PRLogModuleLevel inLevel) { level = inLevel; }
	void	    setOptions(PRLogModuleOptions inOptions) { options = inOptions; }
	PRLogModuleLevel getLogLevel() {return level;}
    const char *getName() { return name; }
	LogModuleObject* getNext() { return next; }
	static LogModuleObject* getAllLogModules() { return allLogModules; }
	FILE *		getFile() const;
};

#define UT_DEFINE_LOG_MODULE(inModuleName)   	  LogModuleObject g__##inModuleName(#inModuleName)
#define UT_LOG(inModule, inLevel, inArgs)         UT_OBJECTLOG(g__##inModule, inLevel, inArgs)
#define UT_EXTERN_LOG_MODULE(inModuleName)		  extern LogModuleObject g__##inModuleName
#define UT_OBJECTLOG(inModuleObject, inLevel, inArgs)	                     \
    (UT_LOG_TEST(inModuleObject, inLevel) ? inModuleObject.logPrint inArgs : 0)
#define UT_LOG_MODULE(inModule)				      g__##inModule
#define UT_SET_LOG_LEVEL(inModuleName, inLevel)   g__##inModuleName.setLogLevel(inLevel)
#define UT_GET_LOG_NAME(inModuleName)             g__##inModuleName.getName()
#define UT_LOG_TEST(inModule, inLevel)		      ((inModule).getLogLevel() >= (inLevel))
#else

typedef int LogModuleObject;							// this needs to stay around because it might be
														// passed as a parameter

#define UT_DEFINE_LOG_MODULE(inModule)					
#define UT_LOG(inModule, inLevel, inArgs)				0
#define UT_EXTERN_LOG_MODULE(inModule)					
#define UT_OBJECTLOG(inModuleObject, inLevel, inArgs)	0
#define UT_SET_LOG_LEVEL(inModuleName, inLevel)
#define UT_GET_LOG_NAME(inModuleName)
#define UT_LOG_TEST(inModule, inLevel)					0

// since LogModule is an int when PR_LOGGING is off, we pass a 0
const int g__Garbage39393 = 0;
#define UT_LOG_MODULE(inModule)							g__Garbage39393


#endif // PR_LOGGING

#endif // _H_LOGMODULE












