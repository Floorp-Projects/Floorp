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

/*
* Handle stepping into funtions
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.api.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

class StepInto implements StepHandler
{
    public StepInto(JSSourceLocation startSourceLocation, JSPC startPC )
    {
        _startSourceLocation = startSourceLocation;
        _startPC             = startPC;
    }

    // implement StepHandler
    public int step( JSThreadState threadState,
                     JSPC pc,
                     JSSourceLocation sourceLocation,
                     Hook hook )
    {
        // different script, gotta stop
        if( ! pc.getScript().equals(_startPC.getScript()) )
            return STOP;

        //
        // NOTE: This little dance is necessary because line numbers for PCs
        // are not always acending. e,g, in:
        //
        //  for(i=0;i<count;i++)
        //     doSomething();
        //
        // The "i++" comes after the "doSomething" call as far as PCs go.
        // But, in JS the line number associated with the "i++" instuction
        // is that of the "for(...". So, the lines for the PC can look like
        // 1,1,2,1,2. Thus we are careful in deciding how to step.
        //

        // definately jumping back
        if( pc.getPC() < _startPC.getPC() &&
            sourceLocation.getLine() != _startSourceLocation.getLine() )
            return STOP;

        // definately jumping forward
        if( sourceLocation.getLine() > _startSourceLocation.getLine() )
            return STOP;

        if(AS.DEBUG){System.out.println( "returning from step_into - not our stop" );}
        return CONTINUE_SEND_INTERRUPT;
    }

    private JSSourceLocation    _startSourceLocation;
    private JSPC                _startPC;
}
