/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.npasw;

import java.io.*;
import java.lang.*;

public class Trace
{
    public static final int TRACE_ON = 1;

    public static int       traceLevel = 2;

    public static void SetTraceLevel( int newTraceLevel )
    {
        traceLevel = newTraceLevel;
    }

    public static void PrintToConsole( String printToConsole )
    {
        System.out.println( printToConsole );
    }

    public static void TRACE_IF( int level, String printToConsole )
    {
        if ( traceLevel >= level )
            System.err.println( printToConsole );
    }

    public static void TRACE( String printToConsole )
    {
        if ( traceLevel >= TRACE_ON )
            System.err.println( printToConsole );
    }

}



