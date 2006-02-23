/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
        // But, in JS the line number associated with the "i++" instruction
        // is that of the "for(...". So, the lines for the PC can look like
        // 1,1,2,1,2. Thus we are careful in deciding how to step.
        //

        // definitely jumping back
        if( pc.getPC() < _startPC.getPC() &&
            sourceLocation.getLine() != _startSourceLocation.getLine() )
            return STOP;

        // definitely jumping forward
        if( sourceLocation.getLine() > _startSourceLocation.getLine() )
            return STOP;

        if(AS.DEBUG){System.out.println( "returning from step_into - not our stop" );}
        return CONTINUE_SEND_INTERRUPT;
    }

    private JSSourceLocation    _startSourceLocation;
    private JSPC                _startPC;
}
