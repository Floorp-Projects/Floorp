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
* Store and compare call chains (for stepping)
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.api.*;

// I may regret this later, but I'm just going to store an array of Scripts.

class CallChain
{
    // compare return codes

    public static final int EQUAL    = 0;
    public static final int CALLER   = 1;
    public static final int CALLEE   = 2;
    public static final int DISJOINT = 3;

    public CallChain( JSThreadState threadState )
    {
        try
        {
            StackFrameInfo[] infoarray = threadState.getStack();
            if( null == infoarray || 0 == infoarray.length )
                return;

            int count = infoarray.length;
            _chain = new Script[count];

            for( int i = 0; i < count; i++ )
                _chain[i] = ((JSPC)(infoarray[i].getPC())).getScript();
        }
        catch( InvalidInfoException e )
        {
            _chain = null;
            return;
        }
    }

    public int compare( CallChain other )
    {
        if( this == other )
            return EQUAL;
        if( null == _chain || null == other || null == other._chain )
            return DISJOINT;

        int count = _chain.length;
        int other_count = other._chain.length;
        if( 0 == count || 0 == other_count )
            return DISJOINT;

        int lesser_count = Math.min( count, other_count );
        int i;
        for( i = 0; i < lesser_count; i++ )
        {
            if( ! _chain[i].equals(other._chain[i]) )
                return DISJOINT;
        }
        if( count > other_count )
            return CALLER;
        if( count < other_count )
            return CALLEE;
        return EQUAL;
    }

    private Script[] _chain;
}
