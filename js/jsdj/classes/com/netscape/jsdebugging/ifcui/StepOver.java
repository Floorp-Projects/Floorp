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
* Handle stepping over function calls
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.api.*;

class StepOver implements StepHandler
{
    public StepOver( JSThreadState startThreadState,
                     JSSourceLocation startSourceLocation,
                     JSPC startPC )
    {
        _callChain = new CallChain(startThreadState);
        _startSourceLocation = startSourceLocation;
        _startPC             = startPC;
    }

    // implement StepHandler
    public int step( JSThreadState threadState,
                     JSPC pc,
                     JSSourceLocation sourceLocation,
                     Hook hook )
    {
        switch( _callChain.compare(new CallChain(threadState)) )
        {
            case CallChain.EQUAL:
                break;
            case CallChain.CALLER:
                return STOP;
            case CallChain.CALLEE:
                return CONTINUE_SEND_INTERRUPT;
            case CallChain.DISJOINT:
                return STOP;
            default:
                if(AS.S)ER.T(false,"coding error in StepOut (missed case)",this);
                break;
        }

        // definately jumping back
        if( pc.getPC() < _startPC.getPC() &&
            sourceLocation.getLine() != _startSourceLocation.getLine() )
            return STOP;

        // definately jumping forward
        if( sourceLocation.getLine() > _startSourceLocation.getLine() )
            return STOP;

        return CONTINUE_SEND_INTERRUPT;
    }

    private CallChain           _callChain;
    private JSSourceLocation    _startSourceLocation;
    private JSPC                _startPC;
}
