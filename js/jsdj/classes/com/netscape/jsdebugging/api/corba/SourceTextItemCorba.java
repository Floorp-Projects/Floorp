/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import com.netscape.jsdebugging.api.*;

public class SourceTextItemCorba
    implements SourceTextItem
{
    /*
    * The 'array' param is an ugly hack. iiop doesn't really refcount the 
    * items, but rather the whole array. As long as this item lives, a
    * ref to the array it camme from must be held. So, we hold it...
    */
    public SourceTextItemCorba(com.netscape.jsdebugging.remote.corba.ISourceTextProvider pro,
                               String url )
    {
        _pro = pro;
        _url = url;
    }

    // implements SourceTextItem
    public String   getURL()        {return _url;}

    public String   getText()
    {
        try
        {
            return _pro.getPageText(_url);
        }
        catch( Exception e )
        {
            // eat it;
            _printException(e, "getText");
            return null;
        }
    }
    public int      getStatus()
    {
        try
        {
            return _pro.getPageStatus(_url);
        }
        catch( Exception e )
        {
            // eat it;
            _printException(e, "getPageStatus");
            return INITED;
        }
    }
    public int      getAlterCount()
    {
        try
        {
            return _pro.getPageAlterCount(_url);
        }
        catch( Exception e )
        {
            // eat it;
            _printException(e, "getAlterCount");
            return 0;
        }
    }

    public boolean  getDirty()      {return _lastAlterCount < getAlterCount();}
    public void     setDirty(boolean b)
    {
        if(b)
            _lastAlterCount-- ;
        else
            _lastAlterCount = getAlterCount();
    }

    private void _printException(Exception e, String funname)
    {
        e.printStackTrace();
        System.out.println("error occured in SourceTextItemCorba." + funname );
    }

    public int hashCode() {return _url.hashCode();}
    public boolean  equals(Object obj)
    {
        try
        {
            return ((SourceTextItemCorba)obj)._url.equals(_url);
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    // data

    private com.netscape.jsdebugging.remote.corba.ISourceTextProvider _pro;
    private String _url;
    private int _lastAlterCount = 0;
}    
