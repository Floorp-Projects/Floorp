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
* Source location (file,line) with persistence support
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.api.JSSourceLocation;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

// XXX implementation note: we currently maintain a string representaion for
// quick lookups. This will change when the Breakpoint becomes more complex...

import netscape.util.*;

public class Location
    implements Comparable, Codable
{

    public Location( JSSourceLocation jssl )
    {
        this( jssl.getURL(), jssl.getLine() );
        if(AS.S)ER.T(null!=jssl,"null JSSourceLocation passed to Location ctor",this);
    }

    public Location( String  url,
                     int     line )
    {
        if(AS.S)ER.T(null!=url,"null url in Location ctor",this);
        if(AS.S)ER.T(line >=0,"negative line in Location ctor",this);

        _url    = url;
        _line   = line;
        _stringRep = new String( _url + "#" + _line );
    }

    // for Codable
    public Location() {}

    public String   getURL()    {return _url; }
    public int      getLine()   {return _line;}

    // override Object
    public String   toString()
    {
        return _stringRep;
    }

    // override Object
    public boolean equals(Object obj)
    {
        if(AS.S)ER.T(obj!=null,"null obect handed to Location equals",this);
        if(AS.S)ER.T(obj instanceof Location,"non-Location obect handed to Location equals",this);

        Location other = (Location) obj;
        if( _stringRep.equals(other._stringRep) )
            return true;
        return false;
    }

    // override Object
    public int hashCode()
    {
        return 13*_url.hashCode() + 17*_line;
    }

    // implement Comparable
    public int compareTo(Object obj)
    {
        if(AS.S)ER.T(obj!=null,"null obect handed to Location compareTo",this);
        if(AS.S)ER.T(obj instanceof Location,"non-Location object handed to Location compareTo",this);

        Location other = (Location) obj;
        int retval = _url.compareTo( other._url );
        if( 0 == retval )
            retval = _line - other._line;
        return retval;
    }

    // implement Codable
    public void describeClassInfo(ClassInfo info)
    {
        info.addField("_url", STRING_TYPE);
        info.addField("_line", INT_TYPE);
    }
    public void encode(Encoder encoder) throws CodingException
    {
        encoder.encodeString("_url", _url);
        encoder.encodeInt("_line", _line);
    }
    public void decode(Decoder decoder) throws CodingException
    {
        _url  = decoder.decodeString("_url");
        _line = decoder.decodeInt("_line");
    }
    public void finishDecoding() throws CodingException
    {
        _stringRep = new String( _url + "#" + _line );
    }

    private String  _url;
    private int     _line;
    private String  _stringRep;
}

