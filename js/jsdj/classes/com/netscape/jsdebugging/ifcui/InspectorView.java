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
* 'View' to interactively do inspection of a tree of data
*/

// when     who     what
// 10/30/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

public class InspectorView
    extends InternalWindow
    implements Observer, Target, TextFilter
{
    private static final int _editDY       = 24;
    private static final int _buttonWidth  = 74;
    private static final int _buttonHeight = 24;
    private static final int _spacerDY     = 5;
    private static final int _spacerDX     = 5;

    private int _nextButtonX;
    private int _nextButtonY;

    public InspectorView( Emperor emperor, Rect rect )
    {
        super(rect);

        _emperor = emperor;
        _inspectorTyrant = emperor.getInspectorTyrant();
        _controlTyrant  = emperor.getControlTyrant();
        _commandTyrant = emperor.getCommandTyrant();

        if(AS.S)ER.T(null!=_inspectorTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);

        _editKeyTextFilter = new EditKeyTextFilter(_commandTyrant);

        _drawer = new InspectorItemDrawer(_emperor.getFixedFont());

        _listview = new InspectorListView(this);

        Size size = contentSize();
        int listwidth   = size.width - (_buttonWidth + (_spacerDX * 3));
        int listtop     = _editDY + (_spacerDY * 2);
        _listheight  = size.height - (listtop + _spacerDY);

        Rect rectEdit = new Rect(_spacerDX, _spacerDY, listwidth, _editDY);
        Rect rectSG   = new Rect(_spacerDX, listtop, listwidth, _listheight);

        _nextButtonX = listwidth + (_spacerDX * 2);
        _nextButtonY = _spacerDY;

        _inspectButton = _addButton( "Inspect",    INSPECT_CMD );
        _nextButtonY +=              _spacerDY * 2;
        _evalButton = _addButton(    "Eval",       EVAL_CMD );
        _addButton(                  "Add Watch",  ADD_WATCH_CMD);
        _addButton(                  "Copy Name",  COPY_NAME_CMD);
        _addButton(                  "Copy Value", COPY_VALUE_CMD);
        _nextButtonY +=              _spacerDY * 4;
        _addButton(                  "Done",       DONE_CMD);

        _textfield = new TextField(rectEdit);
        _textfield.setStringValue("");
        _textfield.setFont(_emperor.getFixedFont());
        _textfield.setFilter(this); // to catch return key and edit cmds
        addSubview(_textfield);

        ScrollGroup sg = new ScrollGroup(rectSG);
        sg.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setContentView( _listview );
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        sg.setBackgroundColor(_emperor.getBackgroundColor());

        setCloseable( false );
        setResizable( false );
        setTitle( "Inspector" );
        addSubview(sg);
        setAutoResizeSubviews(true);

        _listview.setAllowsEmptySelection(true);
        _listview.setAllowsMultipleSelection(false);

        _listview.setTarget(this);
        _listview.setCommand(HIGHLIGHT_CMD);
        _listview.setDoubleCommand(INSPECT_CMD);

        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        setLayer(InternalWindow.PALETTE_LAYER);

        _textfield.setFocusedView();

        boolean stopped = ControlTyrant.STOPPED == _controlTyrant.getState();
        _inspectButton.setEnabled(stopped);
        _evalButton.setEnabled(stopped);

        _inspectorTyrant.addObserver(this);
        _controlTyrant.addObserver(this);

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
        _nextButtonY += _buttonHeight + _spacerDY;
        return button;
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _inspectorTyrant )
        {
            // always refresh if visible...
            if( isVisible() )
                refresh();
        }
        else if ( o == _controlTyrant )
        {
            boolean stopped = ControlTyrant.STOPPED == _controlTyrant.getState();
            _inspectButton.setEnabled(stopped);
            _evalButton.setEnabled(stopped);
        }
    }

    // implement TextFilter interface
    public boolean acceptsEvent(Object o, KeyEvent ke , Vector vec)
    {
        if( ke.isReturnKey() )
        {
            Application.application().performCommandLater(this, INSPECT_CMD, null);
            return false;
        }
        return _editKeyTextFilter.acceptsEvent(o, ke , vec);
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(HIGHLIGHT_CMD) )
        {
            InspectorListItem item = (InspectorListItem)_listview.selectedItem();
            InspectorNodeModel model = item.getModel();
            String fullname = model.getFullName();
            _textfield.cancelEditing();
            _textfield.setStringValue( fullname );
        }
        else if( cmd.equals(INSPECT_CMD) )
        {
            if( ControlTyrant.STOPPED != _controlTyrant.getState() )
                return;

            String s = _textfield.stringValue();
            if( null != s )
            {
                s = s.trim();
                if( 0 != s.length() )
                    _inspectorTyrant.setNewRootNode(s);
            }
            // update done on notification...
        }
        else if( cmd.equals(EVAL_CMD) )
        {
            if( ControlTyrant.STOPPED != _controlTyrant.getState() )
                return;

            String s = _textfield.stringValue();
            if( null != s )
            {
                s = s.trim();
                if( 0 != s.length() )
                {
                    Application.application().performCommandLater(
                            _commandTyrant,
                            _commandTyrant.cmdString(CommandTyrant.EVAL_STRING),
                            s );
                }
            }
        }
        else if( cmd.equals(ADD_WATCH_CMD) )
        {
            String s = _textfield.stringValue();
            if( null != s )
            {
                s = s.trim();
                if( 0 != s.length() )
                {
                    Application.application().performCommandLater(
                            _commandTyrant,
                            _commandTyrant.cmdString(CommandTyrant.WATCH_STRING),
                            s );
                }
            }
        }
        else if( cmd.equals(COPY_NAME_CMD) )
        {
            InspectorListItem item = (InspectorListItem)_listview.selectedItem();
            if( null != item )
                Application.application().performCommandLater(
                        _commandTyrant,
                        _commandTyrant.cmdString(CommandTyrant.COPY_STRING),
                        item.getModel().getFullName() );
        }
        else if( cmd.equals(COPY_VALUE_CMD) )
        {
            InspectorListItem item = (InspectorListItem)_listview.selectedItem();
            if( null != item )
                Application.application().performCommandLater(
                        _commandTyrant,
                        _commandTyrant.cmdString(CommandTyrant.COPY_STRING),
                        item.getModel().getValue() );
        }
        else if( cmd.equals(DONE_CMD) )
        {
            Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.INSPECTOR_SHOW_HIDE),
                    this );
        }
    }

    public synchronized void refresh()
    {
        _emperor.setWaitCursor(true);

        _listview.removeAllItems();

        InspectorNodeModel model = _inspectorTyrant.getRootNode();

        String text = null;
        if( null != model )
        {
            text = model.getName();
            InspectorListItem item = new InspectorListItem( _drawer, model );
            item.setSelectedColor(_emperor.getSelectionColor());
            _listview.addItem( item );

            if( model.hasProperties() )
                expandItem(0,false);

            _listview.selectItemAt(0);
        }

        _textfield.cancelEditing();
        _textfield.setStringValue(text);
        _textfield.setInsertionPoint(null == text ? 0 : text.length());
        _textfield.setFocusedView();

        if( _fixupSizes() )
            layoutView(0,0);
        _listview.draw();

        _emperor.setWaitCursor(false);
    }

    private boolean _fixupSizes()
    {
        int minLeft  = 0;
        int minRight = 0;

        int count = _listview.count();

        for( int i = 0; i < count; i++ )
        {
            InspectorListItem next = (InspectorListItem) _listview.itemAt(i);
            minLeft  = Math.max(minLeft, next.minLeftWidth());
            minRight = Math.max(minRight, next.minRightWidth());
        }
        _drawer.setLeftWidth(minLeft);
        _listview.setBounds(0,0,minLeft+1+minRight,0);
        _listview.sizeToMinSize();
        return true;
    }

    private void _pushItemToTop(int index)
    {
        Rect rectitem = _listview.rectForItemAt(index);
        // if(AS.DEBUG)System.out.println("rectitem.y "+rectitem.y);
        ((ScrollView)(_listview.superview())).scrollBy( 0, -rectitem.y );
    }


    public void expandItem(int index, boolean draw)
    {
        _emperor.setWaitCursor(true);

        InspectorListItem item = (InspectorListItem) _listview.itemAt(index);
        InspectorNodeModel parent = item.getModel();

        parent.expand();

        InspectorNodeModel child = parent.getFirstChild();
        boolean anyChildren = null != child;
        int cur = index;
        while( null != child )
        {
            item.setCollapsed(false);
            InspectorListItem newitem = new InspectorListItem( _drawer, child );
            _listview.insertItemAt( newitem, ++cur );
            newitem.setSelectedColor(_emperor.getSelectionColor());
            child = child.getNextSib();
        }

        if( draw && anyChildren )
        {
            if( _fixupSizes() )
                layoutView(0,0);
            _pushItemToTop(index);
            _listview.draw();
        }

        _emperor.setWaitCursor(false);
    }

    public void collapseItem(int index, boolean draw)
    {
        InspectorListItem item = (InspectorListItem) _listview.itemAt(index);
        InspectorNodeModel parent = item.getModel();

        item.setCollapsed(true);

        int count = _listview.count();

        for( int i = index+1; i < count; i++ )
        {
            InspectorListItem next = (InspectorListItem) _listview.itemAt(index+1);
            if( ! next.getModel().isAncestor(parent) )
                break;
            _listview.removeItemAt(index+1);
        }

        if( draw )
        {
            if( _fixupSizes() )
                layoutView(0,0);
            _listview.scrollItemAtToVisible(index);
            _listview.draw();
        }
    }

    /******************************************/

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

    /******************************************/

    private Emperor             _emperor;
    private InspectorTyrant     _inspectorTyrant;
    private ControlTyrant       _controlTyrant;
    private CommandTyrant       _commandTyrant;
    private InspectorListView   _listview;
    private TextField           _textfield;
    private InspectorItemDrawer _drawer;
    private int                 _listheight;
    private EditKeyTextFilter   _editKeyTextFilter;

    private Button              _inspectButton;
    private Button              _evalButton;

    private static final String HIGHLIGHT_CMD  = "HIGHLIGHT_CMD";
    private static final String INSPECT_CMD    = "INSPECT_CMD";
    private static final String EVAL_CMD       = "EVAL_CMD";
    private static final String ADD_WATCH_CMD  = "ADD_WATCH_CMD";
    private static final String COPY_NAME_CMD  = "COPY_NAME_CMD";
    private static final String COPY_VALUE_CMD = "COPY_VALUE_CMD";
    private static final String DONE_CMD       = "DONE_CMD";


}

/***************************************************************************/

final class InspectorListView extends SmartItemListView
{
    public InspectorListView( InspectorView iv )
    {
        _iv = iv;
    }
    public InspectorView getInspectorView() {return _iv;}

    public void setMousedItem(InspectorListItem item ) {_mousedItem = item;}
    public InspectorListItem getMousedItem() {return _mousedItem;}

    private InspectorView     _iv;
    private InspectorListItem _mousedItem;


}

/***************************************************************************/

final class InspectorListItem extends SmartListItem
{

    public InspectorListItem( InspectorItemDrawer drawer,
                              InspectorNodeModel  model )
    {
        _drawer = drawer;
        _model  = model;

        Size sz = _drawer.sizeForRightText(_model.getValue());
        _height = sz.height;
        _rightWidth = sz.width;
        _leftWidth = _drawer.widthForLeftText(_model.getName(),_model.getDepth());
    }

    public void drawInRect(Graphics g, Rect boundsRect)
    {
        drawBackground(g, boundsRect);

        _drawer.draw( g, boundsRect, this, _model );
    }

    // make this public to be called by our drawer helper
    public void drawStringInRect( Graphics g, String title,
                                  Font titleFont, Rect textBounds,
                                  int justification)
    {
        super.drawStringInRect(g,title,titleFont,textBounds,justification);
    }

    public int minLeftWidth()  {return _leftWidth;}
    public int minRightWidth() {return _rightWidth;}

    public boolean getCollapsed()          {return _collapsed;}
    public void    setCollapsed(boolean b) {_collapsed = b;}

    public InspectorNodeModel getModel() {return _model;}


    // override SmartListItem
    public boolean mouseDown(MouseEvent me)
    {
        if( 1 != me.clickCount()    ||
            me.isAltKeyDown()       ||
            me.isControlKeyDown()   ||
            me.isMetaKeyDown()      ||
            me.isShiftKeyDown()     ||
            ! _hitBox(me) )
        {
            parent().setMousedItem(null);
        }
        else
            parent().setMousedItem(this);
        return super.mouseDown(me);
    }
    public void mouseUp(MouseEvent me)
    {
        if( 1 != me.clickCount()    ||
            me.isAltKeyDown()       ||
            me.isControlKeyDown()   ||
            me.isMetaKeyDown()      ||
            me.isShiftKeyDown()     ||
            ! _hitBox(me)           ||
            this != parent().getMousedItem() )

        {
            parent().setMousedItem(null);
            super.mouseUp(me);
            return;
        }

        InspectorView iv = parent().getInspectorView();

        if( _collapsed )
            iv.expandItem( parent().indexOfItem(this), true );
        else
            iv.collapseItem( parent().indexOfItem(this), true );

        parent().setMousedItem(null);
        super.mouseUp(me);
    }

    private boolean _hitBox(MouseEvent me)
    {
        if( ! _model.hasProperties() )
            return false;
        Rect r = myRect();
        Rect box = _drawer.boxForDepth( _model.getDepth() );
        box.moveBy(r.x, r.y);
        return box.contains(me.x, me.y);
    }

    // override ListItem
    public int minWidth()
    {
        return _leftWidth + _rightWidth + 1;    // XXX
    }


    // override ListItem
    public int minHeight()
    {
        return _height;
    }

    private InspectorListView parent()
    {
        return (InspectorListView) listView();
    }


    private boolean             _collapsed = true;
    private int                 _height;
    private int                 _leftWidth;
    private int                 _rightWidth;
    private InspectorItemDrawer _drawer;
    private InspectorNodeModel  _model;


}

/***************************************************************************/

final class InspectorItemDrawer
{

    public InspectorItemDrawer( Font font)
    {
        _font = font;

        // calc sizes

        _fm = _font.fontMetrics();
        _fontDX = _fm.charWidth('X');
        _fontDY = _fm.height();
        _fontSpacingDY = 1;
        _singleLineItemDY = _fontDY + (_fontSpacingDY*1);

        _lineCenterY = _singleLineItemDY / 2;
        _boxY = _lineCenterY - (_boxDim / 2);

//        System.out.println("_fontDY " + _fontDY);
//        System.out.println("_fontSpacingDY " + _fontSpacingDY);
//        System.out.println("_singleLineItemDY " + _singleLineItemDY);
//        System.out.println("_lineCenterY " + _lineCenterY);
//        System.out.println("_boxY " + _boxY );

    }

    public void draw( Graphics g,
                      Rect boundsRect,
                      InspectorListItem item,
                      InspectorNodeModel model )
    {
        Rect r = new Rect();
        int depth = model.getDepth();


        // draw grids
        g.setColor(_gridColor);
        int horizGridY = boundsRect.y + boundsRect.height - 1;
        int vertGridX = boundsRect.x + _leftDX;
        g.drawLine( boundsRect.x,
                    horizGridY,
                    boundsRect.x + boundsRect.width,
                    horizGridY);

        g.drawLine( vertGridX,
                    boundsRect.y,
                    vertGridX,
                    boundsRect.y + boundsRect.height);


        // draw box

        if( model.hasProperties() )
        {
            g.setColor(_boxColor);
            int lineX = boundsRect.x + _boxX(depth);
            int lineY = _lineCenterY + boundsRect.y;
            g.drawRect( lineX, _boxY + boundsRect.y , _boxDim, _boxDim );
            g.drawLine( lineX + 2, lineY, lineX + _boxDim - 3, lineY );
            if( item.getCollapsed() )
            {
                int armlen = (_boxDim / 2) - 2;
                lineX += _boxDim / 2;
                g.drawLine( lineX, lineY - armlen, lineX, lineY + armlen );
            }
        }

        // draw lines

        g.setColor(_lineColor);
        if( null != model.getParent() )
        {
            int x0 = (boundsRect.x + _boxX(depth-1)) + (_boxDim/2);
            int x1 = (boundsRect.x + _boxX(depth)) - 2;
            int x2 = (boundsRect.x + _boxX(depth)) + _boxDim;
            int y = _lineCenterY + boundsRect.y;

            if( model.hasProperties() )
                g.drawLine( x0, y, x1, y );
            else
                g.drawLine( x0, y, x2, y );

            if( null == model.getNextSib() )
                g.drawLine( x0, boundsRect.y, x0, y );
            else
                g.drawLine( x0, boundsRect.y, x0, boundsRect.y + boundsRect.height);
        }
        if( null != model.getFirstChild() )
        {
            int x =  boundsRect.x + _boxX(depth) + (_boxDim/2);
            int y0 = boundsRect.y + _boxY + _boxDim;
            int y1 = boundsRect.y + boundsRect.height;
            g.drawLine( x, y0, x, y1 );
        }

        // draw in other crossing lines here...

        int crosslineDepth = depth-2;
        InspectorNodeModel p = model.getParent();
        while(null != p)
        {
            if( null != p.getNextSib() )
            {
                int x =  boundsRect.x + _boxX(crosslineDepth) + (_boxDim/2);
                g.drawLine( x, boundsRect.y, x, boundsRect.y + boundsRect.height );
            }
            crosslineDepth-- ;
            p = p.getParent();
        }

        // draw left text

        String name = model.getName();
        if( null != name && 0 != name.length() )
        {
            int x = _boxX(depth) + _boxDim + _fontDX;
            r.setBounds( boundsRect.x + x,
                         boundsRect.y + _fontSpacingDY,
                         boundsRect.width - x,
                         boundsRect.height - _fontSpacingDY );

            if( r.intersects( g.clipRect() ) )
            {
                item.setTextColor(_leftTextColor);
                item.drawStringInRect(g,name,_font,r,Graphics.LEFT_JUSTIFIED);
            }
        }

        // draw right text

        String value = model.getValue();
        if( null != value && 0 != value.length() )
        {
            int x = _leftDX + _spacerDX;
            r.setBounds( boundsRect.x + x,
                         boundsRect.y + _fontSpacingDY,
                         boundsRect.width - x,
                         boundsRect.height - _fontSpacingDY );

            if( r.intersects( g.clipRect() ) )
            {
                if( "function".equals(model.getType()) )
                    item.setTextColor(_rightTextFunctionColor);
                else
                    item.setTextColor(_rightTextColor);
                item.drawStringInRect(g,value,_font,r,Graphics.LEFT_JUSTIFIED);
            }
        }


    }

    public int  getLeftWidth()          {return _leftDX;}
    public void setLeftWidth(int n)     {_leftDX = n;}
    public int  getRightWidth()         {return _rightDX;}
    public void setRightWidth(int n)    {_rightDX = n;}

    public int widthForLeftText( String s, int depth )
    {
        return _boxX(depth) + _boxDim + _fontDX + _fm.stringWidth(s) + _fontDX ;
    }

    public Size sizeForRightText( String s )
    {
        return new Size(_fontDX + _fm.stringWidth(s), _singleLineItemDY);    // XXX
    }

    public Rect boxForDepth( int depth )
    {
        return new Rect( _boxX(depth), _boxY, _boxDim, _boxDim );
    }

    private final int _boxX( int depth ) {return _spacerDX + (_boxDim * depth);}

    /***********/

    private Font        _font;
    private FontMetrics _fm;
    private int         _leftDX;
    private int         _rightDX;
    private int         _fontDX;
    private int         _fontDY;
    private int         _fontSpacingDY;
    private int         _singleLineItemDY;
    private int         _boxY;
    private int         _lineCenterY;

    private static final Color   _leftTextColor          = Color.black;
    private static final Color   _rightTextColor         = Color.red;
    private static final Color   _rightTextFunctionColor = Color.blue;
    private static final Color   _fontColor              = Color.black;
    private static final Color   _gridColor              = Color.blue;
    private static final Color   _lineColor              = Color.gray;
    private static final Color   _boxColor               = Color.black;

    private static final int    _spacerDX = 5;
    private static final int    _spacerDY = 3;

    private static final int    _boxDim = 11;
}
