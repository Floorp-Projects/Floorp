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
import netscape.util.Vector;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;

public class SourceTextProviderLocal
    implements SourceTextProvider
{
    // allow for possible singleton-ness enforcement
    public static SourceTextProvider getSourceTextProvider()
    {
        return new SourceTextProviderLocal();
    }

    private SourceTextProviderLocal()
    {
        try
        {
            PrivilegeManager.enablePrivilege("Debugger");
            _pro = (netscape.jsdebug.JSSourceTextProvider)
                netscape.jsdebug.JSSourceTextProvider.getSourceTextProvider();
        }
        catch( ForbiddenTargetException e )
        {
            // eat it;
        }
    }
    
    // implement netscape.jsdebugger.SourceTextProvider

    public SourceTextItem[] getItems()
    {
        if( null == _pro )
            return null;

        if( null == _items )
            _rebuildItemsArray();
        return _items;
    }
    public void             refreshAll()
    {
        if( null == _pro )
            return;
        _pro.refreshSourceTextVector();
        _rebuildItemsArray();
    }
    public SourceTextItem   findItem(String url)
    {
        if( null == _pro )
            return null;
        if( null == _items )
            _rebuildItemsArray();

        if( null == _items )
            return null;

        for( int i = 0; i < _items.length; i++ )
        {
            if( url.equals(_items[i].getURL()) )
                return _items[i];
        }
        return null;
    }
    public SourceTextItem   loadItem(String url)
    {
        // XXX not implemented...
        return null;
    }

    // this goes through some gyrations to try to reuse existing item wrappers
    private void _rebuildItemsArray()
    {
        SourceTextItemLocal[] olditems = _items;
        _items = null;

        if( null == _pro )
            return;
        try
        {
            int count;
            PrivilegeManager.enablePrivilege("Debugger");
            Vector v = _pro.getSourceTextVector();
            if( null == v || 0 == (count = v.size()) )
                return;
            SourceTextItemLocal[] newitems = new SourceTextItemLocal[count];
 next_item: for( int i = 0; i < count; i++ )
            {
                netscape.jsdebug.SourceTextItem rawItem = 
                            (netscape.jsdebug.SourceTextItem) v.elementAt(i);
                if( null != olditems && 0 != olditems.length )
                {

                    for( int k = 0; k < olditems.length; k++ )
                    {
                        SourceTextItemLocal item = olditems[k];
                        if( item.getWrappedItem() == rawItem )
                        {
                            newitems[i] = item;
                            continue next_item;
                        }
                    }
                }
                newitems[i] = new SourceTextItemLocal(rawItem);
            }
            _items = newitems;
        }
        catch( ForbiddenTargetException e )
        {
            // eat it;
        }
    }


    // data

    private netscape.jsdebug.JSSourceTextProvider _pro   = null;
    private SourceTextItemLocal[]                 _items = null;
}    
