/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;
import java.util.Hashtable;
import java.util.Enumeration;

public class SourceTextManagerImpl
    implements SourceTextManager
{
    public SourceTextManagerImpl()
    {
        _items = new Hashtable();
    }
    
    // implements SourceTextManager
    public synchronized SourceTextItem newItem(String name)
    {
        SourceTextItem item = _findItem(name);

// test code...
//        String cxName = null;
//        cxName = Context.getCurrentContext().toString();
//        System.out.println("newItem for "+
//                           name+(null==item?" *first*":" *replacement*")+
//                           " on context "+cxName);
//        Thread.currentThread().dumpStack();

        if(null != item)
        {
            item.invalidate();
            _items.remove(item);
        }

        item = new SourceTextItemImpl(name);
        _items.put(name,item);

        return item;
    }

    public synchronized SourceTextItem getItem(String name)
    {
        SourceTextItem item = _findItem(name);

        if(null != item && ! item.isValid())
        {
            _items.remove(item);
            item = null;
        }

        if(null == item)
        {
            item = new SourceTextItemImpl(name);
            _items.put(name,item);
        }

        return item;
    }

    public synchronized Enumeration getAllItems()
    {
        // take this opportunity to remove all invalid items...
        Enumeration e = _items.elements();
        while(e.hasMoreElements())
        {
            SourceTextItem item = (SourceTextItem) e.nextElement();
            if(!item.isValid())
                _items.remove(item);
        }

        return _items.elements();
    }

    private final SourceTextItem _findItem(String name)
    {
        return (SourceTextItem) _items.get(name);
    }

    private Hashtable _items;
}


class SourceTextItemImpl
    implements SourceTextItem
{
    SourceTextItemImpl(String name)
    {
        _name = name;
        _textBuffer = new StringBuffer();
        _status = INITED;    
        _alterCount = g_newAlterCount();
    }

    // implements SourceTextItem
    public synchronized boolean append(String text)
    {
        if(_status == INITED || _status == PARTIAL)
        {
            _textBuffer.append(text);
            _status = PARTIAL;
            _up2date = false;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean append(char c)
    {
        if(_status == INITED || _status == PARTIAL)
        {
            _textBuffer.append(c);
            _status = PARTIAL;
            _up2date = false;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean append(char[] buf, int offset, int count)
    {
        if(_status == INITED || _status == PARTIAL)
        {
            _textBuffer.append(buf, offset, count);
            _status = PARTIAL;
            _up2date = false;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean complete()
    {
        if(_status == INITED || _status == PARTIAL)
        {
            _status = COMPLETED;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean abort()
    {
        if(_status == INITED || _status == PARTIAL)
        {
            _status = ABORTED;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean fail()
    {
        if(_status == INITED || _status == PARTIAL)
        {
            _status = FAILED;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean clear()
    {
        if(_status != INVALID)
        {
            _text = null;
            _textBuffer = null;
            _up2date = true;
            _status = CLEARED;
            _alterCount = g_newAlterCount();
            return true;
        }
        return false;
    }

    public synchronized boolean invalidate()
    {
        if(_status != INVALID)
        {
            _text = null;
            _textBuffer = null;
            _up2date = true;
            _status = INVALID;
            return true;
        }
        return false;
    }
    public synchronized String getText()
    {
        if(!_up2date)
        {
            _text = _textBuffer.toString();
            _up2date = true;
        }
        return _text;
    }
    public String  getName()        {return _name;}
    public int     getStatus()      {return _status;}
    public boolean isValid()        {return _status != INVALID;}
    public int     getAlterCount()  {return _alterCount;}

    // this is not part of the interface...
    synchronized void reset()
    {
        _status = INITED;    
        _textBuffer.setLength(0);
        _up2date = false;
        _text = null;
        _alterCount = g_newAlterCount();
    }

    // override Object.hashCode()
    public int hashCode()
    {
        return _name.hashCode();
    }

    // override Object.equals()
    public boolean  equals(Object obj)
    {
        try
        {
            return ((SourceTextItemImpl)obj)._name.equals(_name);
        }
        catch( ClassCastException e )
        {
            return false;
        }
    }

    // override Object.toString()
    public String toString()
    {
        return "SourceTextItem for \""+_name+"\" with status "+_status;
    }

    private static synchronized int g_newAlterCount() {return ++g_alterCount;}

    private String        _name;
    private String        _text;
    private StringBuffer  _textBuffer;
    private int           _status;
    private boolean       _up2date;
    private int           _alterCount;

    private static int    g_alterCount;
}
