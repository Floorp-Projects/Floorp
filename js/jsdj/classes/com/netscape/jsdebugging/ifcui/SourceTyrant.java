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

/*
* 'Model' to manage sources
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.api.*;

public class SourceTyrant
    extends Observable
    implements Observer
{
    public SourceTyrant(Emperor emperor)  throws ForbiddenTargetException
    {
        super();
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();

        PrivilegeManager.enablePrivilege("Debugger");
        _sourceTextProvider = _emperor.getSourceTextProvider();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceTextProvider,"no SourceTextProvider found",this);

        _controlTyrant.addObserver(this);
    }


    public void refreshSourceTextItemVector()
    {
        _emperor.setWaitCursor(true);

        String selectedURL = null;

        if( null != _selectedSourceTextItem )
            selectedURL = _selectedSourceTextItem.getURL();

        PrivilegeManager.enablePrivilege("Debugger");
        synchronized(this)
        {
            _sourceTextProvider.refreshAll();
        }

        if( null != selectedURL )
        {
            setSelectedSourceItem(_sourceTextProvider.findItem(selectedURL));
        }
        _notifyObservers( SourceTyrantUpdate.VECTOR_CHANGED, null );

        _emperor.setWaitCursor(false);
    }

    public SourceTextItem   findSourceItem(String url)
    {
        PrivilegeManager.enablePrivilege("Debugger");
        SourceTextItem sti = _sourceTextProvider.findItem(url);
        // if not found then refresh the provider and retry
        if( null == sti )
        {
            refreshSourceTextItemVector();
            sti = _sourceTextProvider.findItem(url);
        }
        return sti;
    }

    public SourceTextItem[] getSourceTextItemArray()
    {
        PrivilegeManager.enablePrivilege("Debugger");
        return _sourceTextProvider.getItems();
    }
    public SourceTextItem   getSelectedSourceItem()   {return _selectedSourceTextItem;}
    public void             setSelectedSourceItem(SourceTextItem s)
    {
        if( _selectedSourceTextItem == s )
            return;

//        if(AS.DEBUG)System.out.println("sel changed to " + s.getURL() );

        _selectedSourceTextItem = s;
        _notifyObservers( SourceTyrantUpdate.SELECT_CHANGED, s );
    }

    private void _notifyObservers( int type, SourceTextItem item )
    {
        setChanged();
        notifyObservers( new SourceTyrantUpdate(type,item) );
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _controlTyrant )
        {
            if( ControlTyrant.STOPPED == _controlTyrant.getState() )
                refreshSourceTextItemVector();
        }
    }

    public void SetSelectedUrlLineText( String url, int line, String text )
    {
        _urlOfSelectedLine        = url;
        _lineNumberOfSelectedLine = line;
        _selectedText             = text;

        _notifyObservers( SourceTyrantUpdate.SEL_URL_LINE_TEXT_CHANGED, null );

        // the below is for debugging only...
        if(false)
        {
            if( null == url )
            {
                System.out.println( "+++++++++++++++++++++" );
                System.out.println( "selected urllinetext set to null" );
                System.out.println( "---------------------" );
            }
            else
            {
                System.out.println( "+++++++++++++++++++++" );
                System.out.println( "Selection in:" + "\n   " + url );
                System.out.println( "Sel line:"     + "\n   " + line);
                System.out.println( "Changed to:"   + "\n   " + text );
                System.out.println( "---------------------" );
            }
        }
    }

    public String getUrlOfSelectedLine()        {return _urlOfSelectedLine;}
    public int    getLineNumberOfSelectedLine() {return _lineNumberOfSelectedLine;}
    public String getSelectedText()             {return _selectedText;}


    //////////////////////////////////////////////////////////////////
    /* jband - 06/04/97 - new scheme for user souce line adjustment */

    public synchronized void clearAllAdjustmentsForItem( SourceTextItem sti )

    {
        if( null == _adjustments )
            return;
        _adjustments = null;
        _notifyObservers( SourceTyrantUpdate.ADJUSTMENTS_CHANGED, null );
    }

    public synchronized void makeAdjustment( SourceTextItem sti, int line, int offset )
    {
        if(AS.S)ER.T(null!=sti,"bad SourceTextItem in makeAdjustment",this);
        if(AS.S)ER.T(line >=0,"bad line in makeAdjustment",this);

        if( null == _adjustments )
        {
            if( 0 == offset )
                return;
            _adjustments = new Hashtable();
        }

        Vector v = (Vector) _adjustments.get(sti);
        if( null == v )
        {
            if( 0 == offset )
                return;
            v = new Vector();
            _adjustments.put( sti, v );
        }

//        System.out.println( "make adjustment at " + line +" "+ offset );

        int count = v.count();
        for(int i = 0; i < count; i++)
        {
            AdjustmentItem adj = (AdjustmentItem) v.elementAt(i);
            if( adj.line == line )
            {
                if( 0 != offset )
                    adj.offset = offset;
                else
                    v.removeElementAt(i);
                _fixupAdjustmentVector(v);
                _notifyObservers( SourceTyrantUpdate.ADJUSTMENTS_CHANGED, null );
                return;
            }
            if( adj.line > line )
            {
                if( 0 != offset )
                    v.insertElementAt(new AdjustmentItem(line,offset),i);
                _fixupAdjustmentVector(v);
                _notifyObservers( SourceTyrantUpdate.ADJUSTMENTS_CHANGED, null );
                return;
            }
        }
        if( 0 != offset )
            v.addElement(new AdjustmentItem(line,offset));
        _fixupAdjustmentVector(v);
        _notifyObservers( SourceTyrantUpdate.ADJUSTMENTS_CHANGED, null );
    }

    public int getAdjustment( SourceTextItem sti, int line )
    {
        if(AS.S)ER.T(null!=sti,"bad SourceTextItem in getAdjustment",this);
        if(AS.S)ER.T(line >=0,"bad line in getAdjustment",this);

        if( null == _adjustments )
            return 0;
        Vector v = (Vector) _adjustments.get(sti);
        if( null == v )
            return 0;
        int count = v.count();
        for(int i = 0; i < count; i++)
        {
            AdjustmentItem adj = (AdjustmentItem) v.elementAt(i);
            if( adj.line == line )
            {
//                System.out.println( "get adjustment at " + line +" "+ adj.offset );
                return adj.offset;
            }
        }
        return 0;
    }

    public int userLine2SystemLine( SourceTextItem sti, int line )
    {
        if(AS.S)ER.T(null!=sti,"bad SourceTextItem in userLine2SystemLine",this);
        if(AS.S)ER.T(line >=0,"bad line in userLine2SystemLine",this);

        if( null == _adjustments )
            return line;
        Vector v = (Vector) _adjustments.get(sti);
        if( null == v )
            return line;

        int count = v.count();
        for(int i = count-1; i >= 0; i--)
        {
            AdjustmentItem adj = (AdjustmentItem) v.elementAt(i);
            if( (adj.line + adj.cummulative_offset) <= line )
                return line - adj.cummulative_offset;
        }
        return line;
    }

    public int systemLine2UserLine( SourceTextItem sti, int line )
    {
        if(AS.S)ER.T(null!=sti,"bad SourceTextItem in userLine2SystemLine",this);
        if(AS.S)ER.T(line >=0,"bad line in userLine2SystemLine",this);

        if( null == _adjustments )
            return line;
        Vector v = (Vector) _adjustments.get(sti);
        if( null == v )
            return line;

        int adj_line = line;
        int count = v.count();
        for(int i = 0; i < count; i++)
        {
            AdjustmentItem adj = (AdjustmentItem) v.elementAt(i);
            if( adj.line > line )
                break;
            adj_line += adj.offset;
        }
        if( adj_line < 0 )
            return 0;
        return adj_line;
    }

    public int[] getUserAdjustedLineArray( SourceTextItem sti )
    {
        if(AS.S)ER.T(null!=sti,"bad SourceTextItem in getUserAdjustedLineArray",this);

        if( null == _adjustments )
            return null;
        Vector v = (Vector) _adjustments.get(sti);
        if( null == v )
            return null;

        int count = v.count();
        int[] a = new int[count];
        for(int i = 0; i < count; i++)
        {
            AdjustmentItem adj = (AdjustmentItem) v.elementAt(i);
            int val = adj.line + adj.cummulative_offset;
            if( adj.offset < 0 )
                val *= -1;
            a[i] = val;
        }
        return a;
    }


    private void _fixupAdjustmentVector( Vector v )
    {
        int cummulative_offset = 0;
        int count = v.count();
        for(int i = 0; i < count; i++)
        {
            AdjustmentItem adj = (AdjustmentItem) v.elementAt(i);
            if(AS.S)ER.T(adj.offset!=0,"adj.offset == 0",this);
            cummulative_offset += adj.offset;
            adj.cummulative_offset = cummulative_offset;
        }
    }

    //////////////////////////////////////////////////////////////////


    // data...

    private Emperor             _emperor;
    private ControlTyrant       _controlTyrant;
    private SourceTextProvider  _sourceTextProvider;
    private SourceTextItem      _selectedSourceTextItem;

    private String              _urlOfSelectedLine        = null;
    private int                 _lineNumberOfSelectedLine = 0;
    private String              _selectedText             = null;

    private Hashtable           _adjustments = null;
}

class AdjustmentItem
{
    AdjustmentItem(int line, int offset)
    {
        this.line = line;
        this.offset = offset;
    }
    public int line;
    public int offset;
    public int cummulative_offset;
}
