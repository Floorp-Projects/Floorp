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
* 'View' to manage all breakpoints
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
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

public class BreakpointView
    extends InternalWindow
    implements Observer, Target
{
    private static final int _buttonWidth  = 70;
    private static final int _buttonHeight = 24;
    private static final int _buttonYSpace = 5;
    private static final int _buttonXSpace = 5;

    private int _nextButtonX;
    private int _nextButtonY;

    public BreakpointView( Emperor emperor, Rect rect )
    {
        super(rect);

        _emperor = emperor;
        _breakpointTyrant = emperor.getBreakpointTyrant();
        _commandTyrant = emperor.getCommandTyrant();

        if(AS.S)ER.T(null!=_breakpointTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);

        _breakpointTyrant.addObserver(this);

        _listview = new BackgroundHackListView();

        Size size = contentSize();
        int listwidth    = size.width - (_buttonWidth + (_buttonXSpace * 2));
        Rect rectSG = new Rect(0, 0, listwidth, size.height);

        _nextButtonX = listwidth + _buttonXSpace;
        _nextButtonY = _buttonYSpace;

        _addButton( "New",       NEW_CMD );
        _addButton( "Edit",      EDIT_CMD);
        _addButton( "Delete",    DEL_CMD );
        _nextButtonY += _buttonYSpace * 2;
        _addButton( "Done",      DONE_CMD);

        ScrollGroup sg = new ScrollGroup(rectSG);
        sg.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setContentView( _listview );
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        sg.setBackgroundColor(_emperor.getBackgroundColor());

        setCloseable( false );
        setResizable( false );
        setTitle( "Breakpoints" );
        addSubview(sg);
        setAutoResizeSubviews(true);

        _listview.setAllowsEmptySelection(false);
        _listview.setAllowsMultipleSelection(false);

        _listview.setTarget(this);
        _listview.setCommand("");
        _listview.setDoubleCommand(EDIT_CMD);

        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        setLayer(InternalWindow.PALETTE_LAYER);

        refresh();

        layoutView(1,1);
        layoutView(-1,-1);
    }

    private Button _addButton( String title, String cmd )
    {
        Button button = new Button(_nextButtonX,_nextButtonY,
                                   _buttonWidth,_buttonHeight);
        button.setTitle(title);
        button.setTarget(this);
        button.setCommand(cmd);
        addSubview(button);
        _nextButtonY += _buttonHeight + _buttonYSpace;
        return button;
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _breakpointTyrant )
        {
            // always refresh if visible...
            if( isVisible() )
                refresh();
        }
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(NEW_CMD ) )
        {
            newBreakpoint();
        }
        else if( cmd.equals(EDIT_CMD) )
        {
            int index = _listview.selectedIndex();
            if( -1 == index )
                return;

            Breakpoint bp = (Breakpoint) _listview.selectedItem().data();
            if( null == bp )
                return;

            editBreakpoint(bp);
        }
        else if( cmd.equals(DEL_CMD ) )
        {
            int index = _listview.selectedIndex();
            if( index < 0 )
                return;

            Breakpoint bp = (Breakpoint)_listview.itemAt(index).data();

            _breakpointTyrant.removeBreakpoint(bp);
            // will be refreshed upon notification from _breakpointTyrant
        }
        else if( cmd.equals(DONE_CMD) )
        {
            Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.BREAKPOINTS_SHOW_HIDE),
                    this );
        }
    }

    public synchronized void refresh()
    {
        int i;

        Breakpoint selBP = null;
        ListItem selItem = _listview.selectedItem();
        if( null != selItem )
            selBP = (Breakpoint) selItem.data();

        _listview.removeAllItems();

        Font linefont = _emperor.getFixedFont();
        int maxlinelen = 0;

        int itemCount;
        Object[] raw = _breakpointTyrant.getAllBreakpoints();
        if( null == raw || 0 == (itemCount = raw.length) )
            return;

        // create a sorted list

        Vector breakpoints = new Vector(itemCount);
        for( i = 0; i < itemCount; i++ )
            breakpoints.insertElementAt(raw[i],i);

        breakpoints.sort(true);

        // find the max lengths for later formating of each item

        int maxLineno = 0;
        int maxFilenameChars = 0;

        for( i = 0; i < itemCount; i++ )
        {
            Breakpoint bp = (Breakpoint) breakpoints.elementAt(i);
            maxLineno        = Math.max(maxLineno,bp.getLine());
            maxFilenameChars = Math.max(maxFilenameChars,bp.getURL().length());
        }
        int maxStrLenForNum = new Integer(maxLineno).toString().length();

        StringBuffer buf = new StringBuffer();

        // add each item
        for( i = 0; i < itemCount; i++ )
        {
            Breakpoint bp = (Breakpoint) breakpoints.elementAt(i);

            // format text...

            buf.setLength(0);

            // padded line number
            int lineno = bp.getLine();
            buf.append(lineno);

            int pad = maxStrLenForNum - new Integer(lineno).toString().length();
            while( pad-- > 0 )
                buf.insert(0,' ');

            // padded filename
            buf.append(", ");
            buf.append(bp.getURL());
            for( int k = bp.getURL().length()-1; k >= maxFilenameChars; k-- )
                buf.append(' ');

            if( null != bp.getBreakCondition() )
            {
                buf.append(", ");
                buf.append(bp.getBreakCondition());
            }

            ListItem item = new ListItem();
            String text = new String(buf);

//            System.out.println("item number " + i);
//            System.out.println(buf);
//            System.out.println(text);

            maxlinelen = Math.max( maxlinelen, text.length() );
            item.setTitle( text );
            item.setData( bp );
            item.setFont( linefont );
            item.setSelectedColor(_emperor.getSelectionColor());
            _listview.addItem( item );

            if( 0 == i )
                _listview.selectItemAt(i);  // make sure SOMETHING selected

            if( null != selBP && selBP == bp )
            {
                _listview.selectItemAt(i);
                selBP = null; // we only want the first match...
            }
        }

        FontMetrics fm = linefont.fontMetrics();
        _listview.setBounds( 0, 0, (maxlinelen+1) * fm.charWidth('X'),0 );
        _listview.sizeToMinSize();

        layoutView(0,0);
        _listview.draw();
    }

    // from original code...

    public boolean newBreakpoint()
    {
        BreakpointEditorDialog bed =
                new BreakpointEditorDialog( "New Breakpoint",
                                            1, "", "true",
                                            _emperor.getFixedFont(),
                                            true );
        bed.showModally();
        if( bed.okPressed() )
        {
            // create a new BP
            Breakpoint bp;
            Location loc = new Location( bed.getURL(), bed.getLine() );

            String condition = bed.getCondition();
            if( null != condition )
                condition = condition.trim();
            if( 0 == condition.length() || condition.equals("true") )
                condition = null;

            bp = _breakpointTyrant.findBreakpoint(loc);
            if( null == bp )
                bp = _breakpointTyrant.addBreakpoint(loc);

            bp.setBreakCondition(condition);

            _breakpointTyrant.modifiedBreakpoint(bp);

            // select the new item
            int i = _indexOfListItemWithBP(bp);
            if( -1 != i )
                _listview.selectItemAt(i);
            return true;
        }
        return false;

    }

    public boolean editBreakpoint( Breakpoint bp )
    {
        if(AS.S)ER.T(null!=bp,"null Breakpoint", this);

        String condition = bp.getBreakCondition();
        if( null == condition )
            condition = "true";

        BreakpointEditorDialog bed =
                new BreakpointEditorDialog( "Edit Breakpoint",
                                            bp.getLine(),
                                            bp.getURL(),
                                            condition,
                                            _emperor.getFixedFont(),
                                            false );
        bed.showModally();
        if( bed.okPressed() )
        {
            // what changed?

            condition = bed.getCondition();
            if( null != condition )
                condition = condition.trim();
            if( 0 == condition.length() || condition.equals("true") )
                condition = null;

            if( ! bed.getURL().equals(bp.getURL()) ||
                bed.getLine() != bp.getLine() )
            {
                // we need to construct a new breakpoint!
                _breakpointTyrant.removeBreakpoint(bp);
                Location loc = new Location( bed.getURL(), bed.getLine() );
                bp = _breakpointTyrant.addBreakpoint(loc);
            }

            bp.setBreakCondition(condition);
            _breakpointTyrant.modifiedBreakpoint(bp);

            // select the new item
            int i = _indexOfListItemWithBP(bp);
            if( -1 != i )
                _listview.selectItemAt(i);
            return true;
        }
        return false;
    }

    private int _indexOfListItemWithBP( Breakpoint bp )
    {
        for( int i = _listview.count()-1; i >= 0; i-- )
        {
            if( ((Breakpoint) _listview.itemAt(i).data()) == bp )
                return i;
        }
        // failed to find it...
        return -1;
    }


    private Emperor             _emperor;
    private BreakpointTyrant    _breakpointTyrant;
    private CommandTyrant       _commandTyrant;
    private ListView            _listview;

    private static final String NEW_CMD   = "NEW_CMD";
    private static final String EDIT_CMD  = "EDIT_CMD";
    private static final String DEL_CMD   = "DEL_CMD";
    private static final String DONE_CMD  = "DONE_CMD";
}


