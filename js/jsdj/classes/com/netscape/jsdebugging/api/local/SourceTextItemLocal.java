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
