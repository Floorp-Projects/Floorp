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
* 'View' of list of sources
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

public class PageListView
    extends InternalWindow
    implements Observer, Target
{
    public PageListView( Emperor emperor, Rect rect )
    {
        super(rect);

        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _sourceTyrant = emperor.getSourceTyrant();
        _commandTyrant = emperor.getCommandTyrant();
        _stackTyrant    = emperor.getStackTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant     ,"emperor init order problem", this);

        _listview = new BackgroundHackListView();

        Size size = contentSize();
        Rect rectSG = new Rect(0, 0, size.width, size.height-40);

        int buttonWidth  = 64;
        int buttonHeight = 24;
        int buttonY      = rectSG.height + 40/2 - 24/2;
        int button1X      = rectSG.width - (buttonWidth*2 + 4*2);
        int button2X      = rectSG.width - (buttonWidth + 4);

        Button button;
        button = new Button(button1X,buttonY,buttonWidth,buttonHeight);
        button.setTitle("Open");
        button.setTarget(this);
        button.setCommand(OPEN_CMD);
        addSubview(button);

        button = new Button(button2X,buttonY,buttonWidth,buttonHeight);
        button.setTitle("Done");
        button.setTarget(this);
        button.setCommand(CLOSE_CMD);
        addSubview(button);

        ScrollGroup sg = new ScrollGroup(rectSG);
        sg.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setContentView( _listview );
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        sg.setBackgroundColor(_emperor.getBackgroundColor());

        setCloseable( false );
        setResizable( false );
        setTitle( "Open" );
        addSubview(sg);
        setAutoResizeSubviews(true);
//        contentView().setLayoutManager( new MarginLayout() );

        _listview.setAllowsEmptySelection(true);
        _listview.setAllowsMultipleSelection(false);

        _listview.setTarget(_commandTyrant);
        _listview.setCommand(_commandTyrant.cmdString(CommandTyrant.PAGELIST_CLICK));
        _listview.setDoubleCommand(_commandTyrant.cmdString(CommandTyrant.PAGELIST_DBLCLICK));

        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        _sourceTyrant.addObserver(this);
        _controlTyrant.addObserver(this);

        setLayer(InternalWindow.PALETTE_LAYER);

        _refresh();

        layoutView(1,1);
        layoutView(-1,-1);

//        show();
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if( o == _sourceTyrant )
        {
            SourceTyrantUpdate update = (SourceTyrantUpdate)arg;
            switch( update.type )
            {
                case SourceTyrantUpdate.VECTOR_CHANGED :
                    _refresh();
                    break;
                default:
                    break;  // ignore any other types
            }
        }
        else if ( o == _controlTyrant )
        {
            // XXX update list on stop
        }

    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(OPEN_CMD) )
            Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.PAGELIST_DBLCLICK),
                    _listview );
        else if( cmd.equals(CLOSE_CMD) )
            Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.PAGELIST_SHOW_HIDE),
                    this );
    }


    public synchronized void _refresh()
    {
        boolean sort = _emperor.getHostMode() != Emperor.LOCAL;

        _listview.removeAllItems();

        SourceTextItem[] array = _sourceTyrant.getSourceTextItemArray();

        Font linefont = Font.defaultFont();
        int maxlinelen = 0;

        // if stopped, then try to select url of current frame
        // else try to reselect current selection
        String selectedURL = null;
        if( ControlTyrant.STOPPED == _controlTyrant.getState() )
        {
            JSSourceLocation loc = _stackTyrant.getCurrentLocation();
            if( null != loc )
                selectedURL = loc.getURL();
        }
        if( null == selectedURL )
        {
            SourceTextItem sel_item = _sourceTyrant.getSelectedSourceItem();
            if( null != sel_item )
                selectedURL = sel_item.getURL();
        }

        if( null != array )
        {
            synchronized( array )
            {
                for (int i = 0; i < array.length; i++)
                {
                    SourceTextItem src = array[i];
                    ListItem item = new ListItem();

                    String text = src.getURL();
                    maxlinelen = Math.max( maxlinelen, text.length() );
                    item.setTitle( text );
                    item.setData( src );
                    item.setFont( linefont );
                    item.setSelectedColor(_emperor.getSelectionColor());

                    if(sort)
                    {
                        // do an insertion sort...
                        int count = _listview.count();
                        int j;
                        for(j = 0; j < count; j++)
                        {
                            SourceTextItem other = (SourceTextItem)_listview.itemAt(j).data();
                            if( text.compareTo(other.getURL()) < 0 )
                            {
                                _listview.insertItemAt(item,j);
                                break;
                            }
                        }
                        if( j == count )
                            _listview.addItem(item);
                    }
                    else
                    {
                        _listview.addItem(item);
                    }

                    if( null != selectedURL && selectedURL.equals(src.getURL()) )
                        _listview.selectItem(item);
                }
            }
        }
        FontMetrics fm = linefont.fontMetrics();
        _listview.setBounds( 0, 0, (maxlinelen+1) * fm.charWidth('X'),0 );
        _listview.sizeToMinSize();

        layoutView(0,0);
        _listview.draw();
    }

    public static SourceTextItem sourceTextItemForListItem( ListItem li )
    {
        return (SourceTextItem)li.data();
    }

    private Emperor                 _emperor;
    private ControlTyrant           _controlTyrant;
    private SourceTyrant            _sourceTyrant;
    private StackTyrant             _stackTyrant;
    private CommandTyrant           _commandTyrant;
    private ListView                _listview;

    private static final String CLOSE_CMD = "CLOSE_CMD";
    private static final String OPEN_CMD  = "OPEN_CMD";
}



