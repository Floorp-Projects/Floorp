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
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;

public class SourceTextProviderCorba
    implements SourceTextProvider
{
    // allow for possible singleton-ness enforcement
    public static synchronized SourceTextProvider 
    getSourceTextProvider(DebugControllerCorba controller)
    {
        return new SourceTextProviderCorba(controller);
    }

    private SourceTextProviderCorba(DebugControllerCorba controller)
    {
        _controller = controller;
        try
        {
            PrivilegeManager.enablePrivilege("UniversalConnect");

//            String name = _controller.getHostName()+"/NameService/"+wai_name;
//            org.omg.CORBA.Object o = netscape.WAI.Naming.resolve(name);
            org.omg.CORBA.Object o = MyNaming.resolve(_controller.getHostName(),wai_name);
            if( null == o )
            {
                // eat it;
                System.err.println("SourceTextProviderCorba.ctor (null returned by resolve)");
                return;
            }

            _pro = com.netscape.jsdebugging.remote.corba.ISourceTextProviderHelper.narrow(o);
            if( null == _pro )
            {
                // eat it;
                System.err.println("SourceTextProviderCorba.ctor (null returned by narrow)");
                return;
            }
        }
        catch (Exception e)
        {
            // eat it;
            _printException(e, "ctor");
            return;
        }
    }

    // implement com.netscape.jsdebugging.ifcui.SourceTextProvider

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
        try
        {
            _pro.refreshAllPages();
        }
        catch( Exception e )
        {
            // eat it;
            _printException(e, "refreshAll");
        }
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

    private synchronized void _rebuildItemsArray()
    {
        SourceTextItemCorba[] olditems = _items;
        _items = null;

        if( null == _pro )
            return;
        try
        {
            String[] urls = _pro.getAllPages();
            if( null == urls || 0 == urls.length )
                return;
            SourceTextItemCorba[] newitems = new SourceTextItemCorba[urls.length];
 next_item: for( int i = 0; i < urls.length; i++ )
            {
                String url = urls[i];
                if( null != olditems && 0 != olditems.length )
                {
                    for( int k = 0; k < olditems.length; k++ )
                    {
                        SourceTextItemCorba item = olditems[k];
                        if( item.getURL().equals(url) )
                        {
//                            System.out.println( "reusing " + url );
                            newitems[i] = item;
                            continue next_item;
                        }
                    }
                }
//                System.out.println( "+++ new item generated for " + url );
                newitems[i] = new SourceTextItemCorba(_pro,url);
            }
            _items = newitems;
        }
        catch( Exception e )
        {
            // eat it;
            _printException(e, "_rebuildItemsArray");
        }
    }

    private void _printException(Exception e, String funname)
    {
        e.printStackTrace();
        System.out.println("error occured in SourceTextProviderCorba." + funname );
    }

    // data

    private com.netscape.jsdebugging.remote.corba.ISourceTextProvider _pro = null;
    private SourceTextItemCorba[]                         _items = null;

    private DebugControllerCorba _controller;
    private static final String wai_name = "JSSourceTextProvider";
}    
