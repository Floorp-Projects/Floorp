/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
* 'Model' that does data watches
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

public class WatchTyrant
    extends Observable
    implements Observer, PrefsSupport
{
    public WatchTyrant(Emperor emperor)
    {
        super();
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _consoleTyrant  = emperor.getConsoleTyrant();
        _stackTyrant    = emperor.getStackTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_consoleTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant,"emperor init order problem", this);

        _evalStrings = new Vector();

        // test....
//        _evalStrings.addElement( new String( "local" ) );
//        _evalStrings.addElement( new String( "ob" ) );
//        _evalStrings.addElement( new String( "prop" ) );
//        _evalStrings.addElement( new String( "ob[prop]" ) );

        _stackTyrant.addObserver(this);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _stackTyrant )
            evalList();
    }

    public Vector getWatchList()
    {
        return _evalStrings;
    }

    public void addWatchString(String s)
    {
        _evalStrings.addElement(s);
    }

    public void evalList()
    {
        if( null != _stackTyrant.getCurrentFrame() )
            _evalOurList();
    }

    private void _evalOurList()
    {
        int count = _evalStrings.count();
        for( int i = 0; i < count; i++ )
        {
            String s = (String) _evalStrings.elementAt(i);
            if( null != s || 0 != s.length() )
                _consoleTyrant.eval(s);
        }
    }

    // implement PrefsSupport interface
    public void prefsWrite(Archiver archiver)    throws CodingException
    {
        WatchSaver ws = new WatchSaver();
        ws.getFromTyrant(this);
        archiver.archiveRootObject(ws);
    }
    public void prefsRead(Unarchiver unarchiver) throws CodingException
    {
        int id = Util.RootIdentifierForClassName(unarchiver.archive(),
                                      "com.netscape.jsdebugging.ifcui.WatchSaver" );
        if( -1 != id )
        {
            WatchSaver ws = (WatchSaver)
                                    unarchiver.unarchiveIdentifier(id);
            ws.sendToTyrant(this);
        }
    }

    // data...

    private Emperor         _emperor;
    private ControlTyrant   _controlTyrant;
    private ConsoleTyrant   _consoleTyrant;
    private StackTyrant     _stackTyrant;
    private Vector          _evalStrings;
}



