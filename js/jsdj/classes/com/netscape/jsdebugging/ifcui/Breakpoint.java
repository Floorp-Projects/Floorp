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
* Breakpoint with support for break on condition
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import com.netscape.jsdebugging.ifcui.palomar.util.*;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.api.*;

public class Breakpoint
    implements Comparable, Codable
{

    public Breakpoint( String  url,
                       int     line,
                       String  breakCondition )
    {
        this(url,line);
        _breakCondition=breakCondition;
    }

    public Breakpoint( String  url,
                       int     line )
    {
        this(new Location(url,line));
    }
    public Breakpoint( Location loc )
    {
        this();
        _loc = loc;
        if(AS.S)ER.T(null!=loc,"null location in Breakpoint ctor",this);
    }

    public Breakpoint()
    {
        _hooks = new Hashtable();
    }

    // attributes are immutable

    public Location getLocation()   {return _loc;}
    public String   getURL()        {return _loc.getURL(); }
    public int      getLine()       {return _loc.getLine();}

    public String   getBreakCondition()          {return _breakCondition;}
    public void     setBreakCondition(String bc) {_breakCondition=bc;}

    public Hashtable getHooks() {return _hooks;}

    public void putHook(Script script, Hook hook)
    {
        if(AS.S)ER.T(null==_hooks.get(script),"putting an existing hook",this);
        _hooks.put(script, hook);
    }

    public Hook getHook(Script script)
    {
        return (Hook) _hooks.get(script);
    }

    public Hook removeHook(Script script)
    {
        Hook hook = (Hook) _hooks.remove(script);
        if(AS.S)ER.T(null!=hook,"removing a non-existant hook",this);
        return hook;
    }

    // override Object
    public String   toString()
    {
        return _loc.toString();
    }

    // override Object
    public boolean equals(Object obj)
    {
        if(AS.S)ER.T(obj!=null,"null object handed to Breakpoint equals",this);
        if(AS.S)ER.T(obj instanceof Breakpoint,"non-Breakpoint obect handed to Breakpoint equals",this);
        return _loc.equals( ((Breakpoint)obj)._loc );
    }

    // override Object
    public int hashCode()
    {
        return _loc.hashCode();
    }

    // implement Comparable
    public int compareTo(Object obj)
    {
        if(AS.S)ER.T(obj!=null,"null obect handed to Breakpoint compareTo",this);
        if(AS.S)ER.T(obj instanceof Breakpoint,"non-Breakpoint obect handed to Breakpoint compareTo",this);

        Breakpoint other = (Breakpoint) obj;
        return _loc.compareTo(((Breakpoint)obj)._loc);
    }

    // implement Codable
    public void describeClassInfo(ClassInfo info)
    {
        info.addField("_loc", OBJECT_TYPE);
        info.addField("_breakCondition", STRING_TYPE);
    }
    public void encode(Encoder encoder) throws CodingException
    {
        encoder.encodeObject("_loc", _loc);
        encoder.encodeString("_breakCondition", _breakCondition);
    }
    public void decode(Decoder decoder) throws CodingException
    {
        _loc = (Location) decoder.decodeObject("_loc");
        _breakCondition = decoder.decodeString("_breakCondition");
    }
    public void finishDecoding() throws CodingException
    {
    }


    private Hashtable      _hooks;
    private Location       _loc;
    private String         _breakCondition;
}

