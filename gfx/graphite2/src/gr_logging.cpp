/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#include "XmlTraceLog.h"
// #include "graphite2/XmlLog.h"


using namespace graphite2;

extern "C" {


bool graphite_start_logging(GR_MAYBE_UNUSED FILE * logFile, GR_MAYBE_UNUSED GrLogMask mask)
{
#ifdef DISABLE_TRACING
    return false;
#else	//!DISABLE_TRACING
    if (XmlTraceLog::sLog != &XmlTraceLog::sm_NullLog)
    {
        delete XmlTraceLog::sLog;
    }
    XmlTraceLog::sLog = new XmlTraceLog(logFile, "http://projects.palaso.org/graphite2", mask);
    return (XmlTraceLog::sLog != NULL);
#endif		//!DISABLE_TRACING
}

void graphite_stop_logging()
{
#ifndef DISABLE_TRACING
    if (XmlTraceLog::sLog && XmlTraceLog::sLog != &XmlTraceLog::sm_NullLog)
    {
        delete XmlTraceLog::sLog;
        XmlTraceLog::sLog = &XmlTraceLog::sm_NullLog;
    }
#endif		//!DISABLE_TRACING
}


} // extern "C"
