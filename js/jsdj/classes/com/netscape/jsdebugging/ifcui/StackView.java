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
* 'View' for interactive viewing of call stack
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

public class StackView extends InternalWindow
    implements Observer
{
    public StackView( Emperor emperor,
                      Rect rect )
    {
        super(rect);

        _emperor = emperor;
        _stackTyrant    = emperor.getStackTyrant();
        _controlTyrant  = emperor.getControlTyrant();
        _commandTyrant = emperor.getCommandTyrant();
        _sourceTyrant = emperor.getSourceTyrant();

        if(AS.S)ER.T(null!=_stackTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceTyrant,"emperor init order problem", this);

        _listview = new BackgroundHackListView();

        Rect rectSG = new Rect(rect);
        rectSG.moveBy(-rect.x, -rect.y);
        ScrollGroup sg = new ScrollGroup(rectSG);
        sg.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setContentView( _listview );
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        sg.setBackgroundColor(_emperor.getBackgroundColor());

        setCloseable( false );
        setResizable( true );
        setTitle( "Call Stack" );
        addSubview(sg);
        setAutoResizeSubviews(true);
        contentView().setLayoutManager( new MarginLayout() );

        _listview.setAllowsEmptySelection(true);
        _listview.setAllowsMultipleSelection(false);

        _listview.setTarget(_commandTyrant);
        _listview.setCommand(_commandTyrant.cmdString(CommandTyrant.STACKVIEW_CLICK));
        _listview.setDoubleCommand(_commandTyrant.cmdString(CommandTyrant.STACKVIEW_DBLCLICK));
        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        _stackTyrant.addObserver(this);
        _controlTyrant.addObserver(this);
        _sourceTyrant.addObserver(this);

        _refresh();

        layoutView(1,1);
        layoutView(-1,-1);

//        show();

    }

    private synchronized void _refresh()
    {
        // refill all

        _listview.removeAllItems();

        if( ControlTyrant.STOPPED != _controlTyrant.getState() )
        {
            _listview.draw();
            return;
        }

        StackFrameInfo[] array = _stackTyrant.getFrameArray();

        if( null != array && 0 != array.length )
        {
            StackViewItemDrawer drawer = new StackViewItemDrawer(Font.defaultFont());

            int maxwidth = 0;
            for (int i = 0; i < array.length; i++)
            {
                ListItem newlistitem = new StackViewListItem(drawer);

                StackFrameInfo frame = array[i];

                String text;

                if( frame instanceof JSStackFrameInfo )
                {
                    JSPC pc = null;

                    try
                    {
                        pc = (JSPC) frame.getPC();
                    }
                    catch(InvalidInfoException e)
                    {
                        if(AS.S)ER.T(false,"InvalidInfoException in StackViewView",this);
                    }
                    if( null != pc )
                    {
                        JSSourceLocation loc = (JSSourceLocation)pc.getSourceLocation();
                        String function = pc.getScript().getFunction();
                        int lineno = loc.getLine();

                        SourceTextItem sti = _sourceTyrant.findSourceItem(pc.getScript().getURL());
                        lineno = _sourceTyrant.systemLine2UserLine(sti, lineno);

                        if( null != function )
                            text = new String( function + "() line " + lineno );
                        else
                            text = new String( "line" + " " + lineno );
                    }
                    else
                    {
                        text = new String( "BAD_FRAME" );
                    }
                }
                else
                {
                    text = new String( "FORIGN_FRAME" );
                }
                newlistitem.setTitle( text );
                newlistitem.setSelectedColor(_emperor.getSelectionColor());

                // item for current frame will have non-null data
                if( _stackTyrant.getCurrentFrameIndex() == i )
                    newlistitem.setData( this );    // anything non-null
                else
                    newlistitem.setData( null );

                maxwidth = Math.max( maxwidth, newlistitem.minWidth() );
                _listview.addItem( newlistitem );
            }
            _listview.selectItemAt( _stackTyrant.getCurrentFrameIndex() );
            _listview.setBounds(0,0,maxwidth,0);
            _listview.sizeToMinSize();
            layoutView(0,0);
        }
        _listview.draw();
    }


    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _stackTyrant )
        {
            _refresh();
        }
        else if( o == _commandTyrant )
        {
            if( ControlTyrant.STOPPED != _controlTyrant.getState() )
                _refresh();
        }
        else if( o == _sourceTyrant )
        {
            int msg = ((SourceTyrantUpdate)arg).type;
            if( SourceTyrantUpdate.ADJUSTMENTS_CHANGED == msg )
                _refresh();
        }
    }


    // data...

    private Emperor         _emperor;
    private StackTyrant     _stackTyrant;
    private ControlTyrant   _controlTyrant;
    private CommandTyrant   _commandTyrant;
    private SourceTyrant    _sourceTyrant;

    private ListView        _listview;


}

class StackViewListItem extends ListItem
{
    public StackViewListItem(StackViewItemDrawer drawer)
    {
        super();
        _drawer = drawer;
    }

    public int minWidth() {return super.minWidth() + _drawer.width();}

    public void drawInRect(Graphics g, Rect boundsRect)
    {
        drawBackground(g, boundsRect);
        _drawer.draw( g, boundsRect, this, null != data() );
    }

    // make this public to be called by our drawer helper
    public void drawStringInRect( Graphics g, String title,
                                  Font titleFont, Rect textBounds,
                                  int justification)
    {
        super.drawStringInRect(g,title,titleFont,textBounds,justification);
    }

    private StackViewItemDrawer _drawer;


}

class StackViewItemDrawer
{
    public StackViewItemDrawer( Font font )
    {
        _font = font;

        _height = font.fontMetrics().stringHeight();
        int charWidth = font.fontMetrics().charWidth('0');

        Rect rectExec = new Rect( 0, 0, _height/2, _height);

        _width = rectExec.x + rectExec.width;
        int margin = _height / 4;

        rectExec.growBy( -(margin/2), -margin );
        rectExec.moveBy( 0, margin/2 );

        _polyExecPoint  = new Polygon();
        _polyExecPoint.addPoint( rectExec.x, rectExec.y );
        _polyExecPoint.addPoint( rectExec.x, rectExec.y + rectExec.height);
        _polyExecPoint.addPoint( rectExec.x+rectExec.width, rectExec.midY());
        _polyExecPoint.addPoint( rectExec.x, rectExec.y );

        _ptText = new Point( _width, 0 );
    }

    public void draw( Graphics g,
                      Rect boundsRect,
                      StackViewListItem item,
                      boolean isExecPoint )
    {
//        SourceLineItemModel itemModel = (SourceLineItemModel) item.data();

        Rect r = new Rect();

        // draw the main text

        String text = item.title();
        if( null != text && 0 != text.length() )
        {
            r.setBounds( boundsRect.x + _ptText.x,
                         boundsRect.y + _ptText.y,
                         boundsRect.width - _ptText.x,
                         boundsRect.height - _ptText.y );

            item.drawStringInRect(g,text,_font,r,Graphics.LEFT_JUSTIFIED);
        }
        if( isExecPoint )
        {
            int count = 4;
            int[] x = new int[count];
            int[] y = new int[count];

            for( int i = 0; i < count; i++ )
            {
                x[i] = _polyExecPoint.xPoints[i] + boundsRect.x;
                y[i] = _polyExecPoint.yPoints[i] + boundsRect.y;
            }

            g.setColor( Color.blue );
            g.fillPolygon( x,y,count);
        }
    }

    public int width() {return _width;}

    private Font _font;
    private int _width;
    private int _height;
    private Polygon _polyExecPoint;
    private Point   _ptText;
}
