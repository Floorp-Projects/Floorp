/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.jslogger;

import netscape.jsdebug.*;

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
