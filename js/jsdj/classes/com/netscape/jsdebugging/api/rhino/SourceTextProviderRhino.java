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

package com.netscape.jsdebugging.api.rhino;

import com.netscape.jsdebugging.api.*;
import netscape.util.Vector;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import java.util.Enumeration;

public class SourceTextProviderRhino
    implements SourceTextProvider
{
    public static boolean setGlobalSourceTextManager(com.netscape.javascript.SourceTextManager manager)
    {
        if(null != _pro)
            return false;
        _pro = manager;
        return true;
    }

    // allow for possible singleton-ness enforcement
    public static SourceTextProvider getSourceTextProvider()
    {
        return new SourceTextProviderRhino();
    }

    private SourceTextProviderRhino()
    {
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
//        _pro.refreshSourceTextVector();
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
        SourceTextItemRhino[] olditems = _items;
        _items = null;

        if( null == _pro )
            return;
        try
        {
            int count;
            PrivilegeManager.enablePrivilege("Debugger");

            // XXX this is ugly, but for now we build a Vecter from the 
            // Enumeration so that we can reuse the old code below
            Vector v = new Vector();
            Enumeration e = _pro.getAllItems();

            while(e.hasMoreElements())
                v.addElement(e.nextElement());

            if( null == v || 0 == (count = v.size()) )
                return;
            SourceTextItemRhino[] newitems = new SourceTextItemRhino[count];
 next_item: for( int i = 0; i < count; i++ )
            {
                com.netscape.javascript.SourceTextItem rawItem = 
                            (com.netscape.javascript.SourceTextItem) v.elementAt(i);
                if( null != olditems && 0 != olditems.length )
                {

                    for( int k = 0; k < olditems.length; k++ )
                    {
                        SourceTextItemRhino item = olditems[k];
                        if( item.getWrappedItem().equals(rawItem) )
                        {
                            if( item.getWrappedItem() != rawItem )
                                item.setWrappedItem(rawItem);
                            newitems[i] = item;
                            continue next_item;
                        }
                    }
                }
                newitems[i] = new SourceTextItemRhino(rawItem);
            }
            _items = newitems;
        }
        catch( ForbiddenTargetException e )
        {
            // eat it;
        }
    }


    // data

    private static com.netscape.javascript.SourceTextManager _pro   = null;
    private SourceTextItemRhino[]                 _items = null;
}    
