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
