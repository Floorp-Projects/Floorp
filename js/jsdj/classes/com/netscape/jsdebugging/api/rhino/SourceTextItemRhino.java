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

package com.netscape.jsdebugging.api.rhino;

import com.netscape.jsdebugging.api.*;

public class SourceTextItemRhino
    implements SourceTextItem
{
    public SourceTextItemRhino(com.netscape.javascript.SourceTextItem sti)
    {
        _sti = sti;
        //
        // XXX Hack to deal with the fact that JSScript 'filenames' for
        // urls containing query parts are truncated at '?', while sources 
        // contain the full URL. (do the same for '#')

        int index;
        String url = _sti.getName();
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
    public int      getAlterCount() {return _sti.getAlterCount();}

    public boolean  getDirty()      {return _lastAlterCount < getAlterCount();}
    public void     setDirty(boolean b)
    {
        if(b)
            _lastAlterCount-- ;
        else
            _lastAlterCount = getAlterCount();
    }

    public int hashCode() {return _sti.getName().hashCode();}
    public boolean  equals(Object obj)
    {
        try
        {
            return ((SourceTextItemRhino)obj)._sti == _sti;
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    com.netscape.javascript.SourceTextItem getWrappedItem() {return _sti;}
    void setWrappedItem(com.netscape.javascript.SourceTextItem sti) {_sti = sti;}

    // data

    private String _url;
    private com.netscape.javascript.SourceTextItem _sti = null;
    private int _lastAlterCount = 0;
}    
