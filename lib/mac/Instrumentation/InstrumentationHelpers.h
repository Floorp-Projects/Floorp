/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser  sfraser@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/*

	About this file
	---------------
	
	This file contains some handy macros, and a stack-based class that makes
	instrumenting functions with Apple's Instrumentation SDK easier.
	
	Instrumentation SDK
	-------------------
	
	The Instrumentation SDK allows you to do code performance analysis,
	collecting time-based data, and other data by putting instrumentation
	points in your code, and running.
	
	You can get the instrumentation SDK from:
	
	ftp://ftp.apple.com/developer/Development_Kits/Instrumentation_SDK.hqx
	
	To find out how to use the Instrumentation Toolkit, read the documentation
	that comes with the SDK. I'm not going to explain all that here.
	
	Setting up your tree
	--------------------
	
	Put the headers and libraries from the Instrumentation SDK into
	a folder in your CodeWarrior 'Mac OS Support' folder.
	
	How to use
	----------
	
	In C++ code, the following macros can be used thusly:
	
	NS_IMETHODIMP nsBigThing::SlowFunction()
	{
	  INST_TRACE("SomeLocationDescription");			// Descriptive label. Don't use :: here.
	
	  // code that might return anywhere.
	  ...
	
	  return NS_OK;
	}
	
  Because the INST_TRACE macro makes a stack-based StInstrumentationLog,
  exit from the function will be traced wherever it occurs.
  
  You will also need to add the "InstrumentationLib" stub library to the
  project.
  
  Such instrumentation points will give you data for the
  'Trace Time Line Viewer' in the viewer. There are many other types
  of instrumentation data you can collect; see the docs in the SDK
  for more information.
  
  Stalking the wild time sink
  ---------------------------
  
  Your general strategy when using the Instrumentation tools to
  track down performance problems should be to start at the high
  level, then drill down into subroutines. You want to avoid 
  instrumenting routines that are called hundreds of thousands
  of times, because you'll end up with massive data files. Rather,
  instrument their calling routines to get a good sense of where
  the time goes.
  
  This kind of performance analysis does not replace the more
  traditional profiling tools. Rather, it allows you to analyse
  performance problems in terms of behaviour and timing, rather
  than a simply overview of % of time spent in different routines.

*/

#include <InstrumentationMacros.h>

#ifdef __cplusplus
/* Stack-based class to do logging */

class StInstrumentationLog
{
public:
		StInstrumentationLog(const char* traceName, InstTraceClassRef &ioTraceClassRef)
		{
			if (ioTraceClassRef == 0)
			{
				if (InstCreateTraceClass( kInstRootClassRef, traceName, 0, kInstEnableClassMask, &ioTraceClassRef) != noErr)
				{
					DebugStr("\pFailed to make instrumentation trace class");
					return;
				}
			}
			
			mTraceClassRef = ioTraceClassRef;
			mEventTag = InstCreateEventTag();
			InstLogTraceEvent(mTraceClassRef, mEventTag, kInstStartEvent);
		}
		
		~StInstrumentationLog()
		{
			InstLogTraceEvent(mTraceClassRef, mEventTag, kInstEndEvent);
		}
		
		void LogMiddleEvent()
		{
			InstLogTraceEvent(mTraceClassRef, mEventTag, kInstMiddleEvent);
		}
		
		void LogMiddleEventWithData(const char* inFormatString, void* inEventData)
		{
			InstDataDescriptorRef	dataDesc;
			InstCreateDataDescriptor(inFormatString, &dataDesc);
			InstLogTraceEventWithData(mTraceClassRef, mEventTag, kInstMiddleEvent, dataDesc, inEventData);
			InstDisposeDataDescriptor(dataDesc);
		}
		
protected:

	InstTraceClassRef	mTraceClassRef;
	InstEventTag		mEventTag;
};

#define INST_TRACE(n)					static InstTraceClassRef __sTrace = 0;  StInstrumentationLog  traceLog((n), __sTrace)
#define INST_TRACE_MIDDLE			do { traceLog.LogMiddleEvent(); } while(0)
#define INST_TRACE_DATA(s, d)	do { traceLog.LogMiddleEventWithData((s), (d)); } while (0)

#endif /* __cplusplus */
