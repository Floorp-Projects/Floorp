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

package com.netscape.jsdebugging.api.local;

import com.netscape.jsdebugging.api.*;

public class SourceTextItemLocal
    implements SourceTextItem
{
    public SourceTextItemLocal(netscape.jsdebug.SourceTextItem sti)
    {
        _sti = sti;
        //
        // XXX Hack to deal with the fact that JSScript 'filenames' for
        // urls containing query parts are truncated at '?', while sources 
        // contain the full URL. (do the same for '#')

        int index;
        String url = _sti.getURL();
        if( -1 != (index = url.indexOf('?')) ||
            -1 != (index = url.indexOf('#')) )
            _url = url.substring(0,index);
        else
            _url = url;
    }

    // implements SourceTextItem
    public String   getURL()        {return _url;}
    public String   getText()       {return _sti.getText();}
    public int      getStatus()     {return _sti.getStatus();}
    public int      getAlterCount()
    {
        if( _sti.getDirty() )
        {
            _sti.setDirty(false);
            _lastAlterCount++;
        }
        return _lastAlterCount;
    }

    public boolean  getDirty()      {return _sti.getDirty();}
    public void     setDirty(boolean b) {_sti.setDirty(b);}

    public int hashCode() {return _sti.getURL().hashCode();}
    public boolean  equals(Object obj)
    {
        try
        {
            return ((SourceTextItemLocal)obj)._sti == _sti;
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    public netscape.jsdebug.SourceTextItem getWrappedItem() {return _sti;}

    // data

    private String _url;
    private netscape.jsdebug.SourceTextItem _sti = null;
    private int _lastAlterCount = 0;
}    
