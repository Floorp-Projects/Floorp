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

