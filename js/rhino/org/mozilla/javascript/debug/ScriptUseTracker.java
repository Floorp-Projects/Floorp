/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript.debug;

import org.mozilla.javascript.debug.util.*;

public final class ScriptUseTracker
{
    public ScriptUseTracker()
    {
        _active = new CList();
        _fallow = new CList();
    }

    public int getActiveCount() {return _activeCount;}
    public int getFallowCount() {return _fallowCount;}

    // there is a lot of trust here :)

    public void addActiveScript(Script s)
    {
        if(AS.S)ER.T(!DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(!DEBUG_inList(s,_fallow),"illegal action",this);

        _active.appendLink(s);
        _activeCount++ ;
    }
    public void addFallowScript(Script s)
    {
        if(AS.S)ER.T(!DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(!DEBUG_inList(s,_fallow),"illegal action",this);

        _fallow.appendLink(s);
        _fallowCount++ ;
    }
    public void removeActiveScript(Script s)
    {
        if(AS.S)ER.T(DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(!DEBUG_inList(s,_fallow),"illegal action",this);

        CList.removeAndInitLink(s);
        _activeCount-- ;
    }
    public void removeFallowScript(Script s)
    {
        if(AS.S)ER.T(!DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(DEBUG_inList(s,_fallow),"illegal action",this);

        CList.removeAndInitLink(s);
        _fallowCount-- ;
    }
    public void moveScriptFromFallowToActive(Script s)
    {
        if(AS.S)ER.T(!DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(DEBUG_inList(s,_fallow),"illegal action",this);

        CList.removeAndInitLink(s);
        _fallowCount-- ;
        _active.appendLink(s);
        _activeCount++ ;
    }
    public void moveScriptFromActiveToFallowHead(Script s)
    {
        if(AS.S)ER.T(DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(!DEBUG_inList(s,_fallow),"illegal action",this);

        CList.removeAndInitLink(s);
        _activeCount-- ;
        _fallow.insertLink(s);
        _fallowCount++ ;
    }
    public void moveScriptFromActiveToFallowTail(Script s)
    {
        if(AS.S)ER.T(DEBUG_inList(s,_active),"illegal action",this);
        if(AS.S)ER.T(!DEBUG_inList(s,_fallow),"illegal action",this);

        CList.removeAndInitLink(s);
        _activeCount-- ;
        _fallow.appendLink(s);
        _fallowCount++ ;
    }

    public Script getFallowHead()
    {
        if( CList.isEmpty(_fallow) )
            return  null;
        return (Script) _fallow.head();
    }

    public CList getActiveList() {return _active;}
    public CList getFallowList() {return _fallow;}

    public String toString()
    {
        return "ScriptUseTracker with "+_activeCount+" active and "+_fallowCount+" fallow Scripts";
    }

    private boolean DEBUG_inList(ICListElement e, CList l)
    {
        if(AS.S)
        {
            for(ICListElement c = l.head(); c != l; c = c.getNext())
                if(c == e)
                    return true;
        }
        return false;
    }

    private CList _active;
    private CList _fallow;
    private int _activeCount;
    private int _fallowCount;
}
