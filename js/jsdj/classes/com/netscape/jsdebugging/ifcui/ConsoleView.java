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
* 'View' for console
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
import com.netscape.jsdebugging.api.*;


public class ConsoleView
    extends InternalWindow
    implements Observer, ConsolePrinter, Target, TextFilter
{
    public ConsoleView( Emperor emperor, Rect rect )
    {
        super(rect);

        _emperor = emperor;
        _controlTyrant = emperor.getControlTyrant();
        _consoleTyrant = emperor.getConsoleTyrant();
        _commandTyrant = emperor.getCommandTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_consoleTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);

        _editKeyTextFilter = new EditKeyTextFilter(_commandTyrant);

        _listview = new NoSelectListView();
        _textfield = new TextField(0,0,rect.width-1,25);

        _linefont = _emperor.getFixedFont();

        Rect rectSG = new Rect(rect);
        rectSG.moveBy(-rect.x, -rect.y);

        ScrollGroup sg1 = new ScrollGroup(rectSG);
        sg1.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg1.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg1.setContentView( _listview );
        sg1.setAutoResizeSubviews(true);
        sg1.contentView().setLayoutManager( new MarginLayout() );
        sg1.setBackgroundColor(_emperor.getBackgroundColor());

        ScrollGroup sg2 = new ScrollGroup(rectSG);
        sg2.setHorizScrollBarDisplay( ScrollGroup.NEVER_DISPLAY );
        sg2.setVertScrollBarDisplay(  ScrollGroup.NEVER_DISPLAY );
        sg2.setContentView( _textfield );
        sg2.setAutoResizeSubviews(true);
        sg2.contentView().setLayoutManager( new MarginLayout() );
        sg2.setBackgroundColor(_emperor.getBackgroundColor());

        BoxView sv = new BoxView(SplitterView.VERT);

        sv.addFlexibleView(sg1);
        sv.addFixedView(sg2,30);

        setCloseable( false );  // XXX nice to allow close and track in menu
        setResizable( true );
        setTitle( "Console" );
        addSubview(sv);
        setAutoResizeSubviews(true);
        contentView().setLayoutManager( new MarginLayout() );

        _listview.setAllowsEmptySelection(true);
        _listview.setAllowsMultipleSelection(false);
        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        _textfield.setTextColor(Color.black);
        _textfield.setSelectionColor(_emperor.getSelectionColor());
        _setTextFieldEnabledState();

//        _textfield.setWrapsContents(true);
//        _textfield.setJustification(Graphics.LEFT_JUSTIFIED);
        _textfield.setSelectable(true);
        _textfield.setFont(_linefont);
        _textfield.setHorizResizeInstruction( View.WIDTH_CAN_CHANGE );
        _textfield.setFilter(this); // to catch return key and edit cmds

        _redrawTimer = new Timer(this, null, 100 );
        _redrawTimer.setCommand(DRAW_CMD);
        _redrawTimer.setRepeats(false);
        _redrawTimer.setCoalesce(true);

        _lastLineTerminated = true;
        _consoleTyrant.setPrinter(this);

        setFocusedView(_textfield);

        layoutView(1,1);
        layoutView(-1,-1);

//        show();

        _controlTyrant.addObserver(this);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _controlTyrant )
            _setTextFieldEnabledState();
    }

    private void _setTextFieldEnabledState()
    {
        if( ControlTyrant.STOPPED == _controlTyrant.getState() )
        {
            _textfield.setBackgroundColor(_emperor.getBackgroundColor());
            _textfield.setEditable(true);
        }
        else
        {
            _textfield.setBackgroundColor(_emperor.getDisabledBackgroundColor());
            _textfield.setEditable(false);
        }
    }


    // implement ConsolePrinter interface
    public synchronized void print(String stringToPrint, boolean isOutput)
    {
        StringBuffer buf;

        if( null == stringToPrint || 0 == stringToPrint.length() )
            return;

        if( ! _lastLineTerminated )
        {
            // get the text of the last item in the list

            int listItemIndex = _listview.count() - 1;
            if(AS.S)ER.T(listItemIndex>=0,"bad listview item",this);

            buf = new StringBuffer( _listview.itemAt(listItemIndex).title() );
            _listview.removeItemAt(listItemIndex);
        }
        else
            buf = new StringBuffer();

        int len = stringToPrint.length();
        for( int i = 0; i < len; i++ )
        {
            char c = stringToPrint.charAt(i);
            if( c == '\n' )
            {
                _addStringToList( buf.toString(), isOutput );
                buf.setLength(0);
            }
            else
                buf.append(c);
        }

        // add the left overs
        if( 0 != buf.length() )
            _addStringToList( buf.toString(), isOutput );

        _lastLineTerminated = ('\n' == stringToPrint.charAt(len-1));

        // do defered draw
        _redrawTimer.start();
    }

    private void _addStringToList(String text, boolean isOutput)
    {
        text = Util.expandTabs(text,8);

        ListItem item = new ListItem();
        item.setTitle( text );
        item.setFont( _linefont );
        item.setSelectedColor(_emperor.getSelectionColor());
        item.setTextColor( isOutput ? Color.red : Color.black );
        _listview.addItem( item );

        _maxlinelen = Math.max( _maxlinelen, text.length() );
    }

    // implement Target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(EVAL_CMD) )
        {
            String text = _textfield.stringValue();
            _consoleTyrant.eval(text);
            _textfield.setStringValue("");
            _textfield.setFocusedView();
        }
        else if( cmd.equals(DRAW_CMD) )
        {
            // do defered draw
            // draw and layout

            FontMetrics fm = _linefont.fontMetrics();
            _listview.setBounds( 0, 0, (_maxlinelen+1) * fm.charWidth('X'),0 );
            _listview.sizeToMinSize();
            this.layoutView(0,0);

            int last = _listview.count() - 1;
            if( last >= 0 )
                _listview.scrollItemAtToVisible(last);
            _listview.draw();
        }
    }

    // implement TextFilter interface
    public boolean acceptsEvent(Object o, KeyEvent ke , Vector vec)
    {
        if( ke.isReturnKey() )
        {
            Application.application().performCommandLater(this, EVAL_CMD, null);
            return false;
        }
        return _editKeyTextFilter.acceptsEvent(o, ke , vec);
    }

    public synchronized void clear()
    {
        _listview.removeAllItems();
        _lastLineTerminated = true;
        _maxlinelen = 0;

        // force a correct redraw
        performCommand( DRAW_CMD, null );
    }

    public void didBecomeMain()
    {
        _commandTyrant.refreshCmdStatesAndNotifyObservers();
        super.didBecomeMain();
    }

    public void didResignMain()
    {
        _commandTyrant.refreshCmdStatesAndNotifyObservers();
        super.didResignMain();
    }

    public boolean canCopy()
    {
        if( null == _textfield )
            return false;
        if( ! _textfield.isEditable() )
            return false;
        if( ! isMain() )
            return false;
        // the problem with this is that we're not getting notified when
        // a selection does happen. Rather than failing to notify commandTyrant
        // when we later become copyable, we'd rather lie and say that we're
        // copyable even if we aren't really (yet)...
        //
        // XXX fixing this with a proper notification would be good.
        //
//        return _textfield.hasSelection();
        return true;
    }

    public String  copy()
    {
        if( ! canCopy() )
            return null;
        return _textfield.stringForRange( _textfield.selectedRange() );
    }

    public String  cut()
    {
        if( ! canCopy() )
            return null;
        Range r = _textfield.selectedRange();
        String retval = _textfield.stringForRange(r);
        _textfield.replaceRangeWithString(r, "");
        _textfield.setInsertionPoint(r.index);
        return retval;
    }

    public boolean canPaste()
    {
        if( null == _textfield )
            return false;
        if( ! _textfield.isEditable() )
            return false;
        if( ! isMain() )
            return false;
        return true;
    }

    public void    paste(String s)
    {
        if( null == s || 0 == s.length() )
            return;
        if( ! canPaste() )
            return;
        Range r = _textfield.selectedRange();
        _textfield.replaceRangeWithString(r, s);
        _textfield.setInsertionPoint(r.index + s.length());
    }

    private Emperor                 _emperor;
    private ControlTyrant           _controlTyrant;
    private ConsoleTyrant           _consoleTyrant;
    private CommandTyrant           _commandTyrant;
    private ListView                _listview;
    private TextField               _textfield;

    private Timer                   _redrawTimer;

    private int                     _maxlinelen = 0;
    private Font                    _linefont;
    private boolean                 _lastLineTerminated;
    private EditKeyTextFilter       _editKeyTextFilter;

    private static final String     EVAL_CMD = "EVAL_CMD";
    private static final String     DRAW_CMD = "DRAW_CMD";
}

class NoSelectListView extends BackgroundHackListView
{
    public NoSelectListView()               {super();}
    public boolean mouseDown(MouseEvent me) {return false;}
    public void mouseDragged(MouseEvent me) {}
    public void mouseUp(MouseEvent me)      {}
}
