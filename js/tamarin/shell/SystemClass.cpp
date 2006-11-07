/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */


#include "avmshell.h"

#include <stdlib.h>

namespace avmshell
{
	BEGIN_NATIVE_MAP(SystemClass)
		NATIVE_METHOD(avmplus_System_exit, SystemClass::exit)
		NATIVE_METHOD(avmplus_System_exec, SystemClass::exec)
		NATIVE_METHOD(avmplus_System_getAvmplusVersion, SystemClass::getAvmplusVersion)
		NATIVE_METHOD(avmplus_System_trace, SystemClass::trace)
		NATIVE_METHOD(avmplus_System_write, SystemClass::write)
		NATIVE_METHOD(avmplus_System_debugger, SystemClass::debugger)
		NATIVE_METHOD(avmplus_System_isDebugger, SystemClass::isDebugger)
		NATIVE_METHOD(avmplus_System_getTimer, SystemClass::getTimer)
		NATIVE_METHOD(avmplus_System_readLine, SystemClass::readLine)
		NATIVE_METHOD(avmplus_System_private_getArgv, SystemClass::getArgv)
	END_NATIVE_MAP()
					  
	SystemClass::SystemClass(VTable *cvtable)
		: ClassClosure(cvtable)
    {
		Shell* core = (Shell*)this->core();
		if (core->systemClass == NULL) {
			core->systemClass = this;
		}
		
		createVanillaPrototype();

		// initialTime: support for getTimer
		// todo note this is currently routed to the performance counter
		// for benchmark purposes.
		#ifdef PERFORMANCE_GETTIMER
		initialTime = MMgc::GC::getPerformanceCounter();
		#else
		initialTime = OSDep::currentTimeMillis();		
		#endif // PERFORMANCE_GETTIMER

	}

	SystemClass::~SystemClass()
	{
		initialTime = 0;
	}

	void SystemClass::exit(int status)
	{
		::exit(status);
	}

	int SystemClass::exec(Stringp command)
	{
		if (!command) {
			toplevel()->throwArgumentError(kNullArgumentError, "command");
		}
		UTF8String *commandUTF8 = command->toUTF8String();
		return system(commandUTF8->c_str());
	}
	
	Stringp SystemClass::getAvmplusVersion()
	{
		return core()->newString(AVMPLUS_VERSION_USER " " AVMPLUS_BUILD_CODE);
	}

	void SystemClass::write(Stringp s)
	{
		if (!s)
			toplevel()->throwArgumentError(kNullArgumentError, "string");
		core()->console << s;
	}

	void SystemClass::trace(ArrayObject* a)
	{
		if (!a)
			toplevel()->throwArgumentError(kNullArgumentError, "array");
		AvmCore* core = this->core();
		PrintWriter& console = core->console;
		for (int i=0, n = a->getLength(); i < n; i++)
		{
			if (i > 0)
                console << ' ';
			Stringp s = core->string(a->getUintProperty(i));
			for (int j = 0; j < s->length(); j++)
			{
				wchar c = (*s)[j];
				// '\r' gets converted into '\n'
				// '\n' is left alone
				// '\r\n' is left alone
				if (c == '\r')
				{
					if (((j+1) < s->length()) && (*s)[j+1] == '\n')
					{
						console << '\r';	
						j++;
					}

					console << '\n';
				}
				else
				{
					console << c;
				}
			}
		}
		console << '\n';
	}

	void SystemClass::debugger()
	{
		#ifdef DEBUGGER
		core()->debugger->enterDebugger();
		#endif
	}

	bool SystemClass::isDebugger()
	{
		#ifdef DEBUGGER
		return true;
		#else
		return false;
		#endif
	}

	unsigned SystemClass::getTimer()
	{
#ifdef PERFORMANCE_GETTIMER
		double time = ((double) (MMgc::GC::getPerformanceCounter() - initialTime) * 1000.0 /
					   (double)MMgc::GC::getPerformanceFrequency());
		return (uint32)time;
#else
		return (uint32)(OSDep::currentTimeMillis() - initialTime);
#endif /* PERFORMANCE_GETTIMER */

    }

	int SystemClass::user_argc;
	char **SystemClass::user_argv;

	ArrayObject * SystemClass::getArgv()
	{
		// get VTable for avmplus.System
		Toplevel *toplevel = this->toplevel();
		AvmCore *core = this->core();

		ArrayObject *array = toplevel->arrayClass->newArray();
		for(int i=0; i<user_argc;i++)
			array->setUintProperty(i, core->newString(user_argv[i])->atom());

		return array;
	}

	Stringp SystemClass::readLine()
	{
		AvmCore* core = this->core();
		Stringp s = core->kEmptyString;
		wchar wc[64];
		int i=0;
		for (int c = getchar(); c != '\n' && c != EOF; c = getchar())
		{
			wc[i++] = c;
			if (i == 63) {
				wc[i] = 0;
				s = core->concatStrings(s, core->newString(wc));
				i = 0;
			}
		}
		if (i > 0) {
			wc[i] = 0;
			s = core->concatStrings(s, core->newString(wc));
		}
		return s;
	}
}
