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
* 'View' for interactive viewing of source code
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
import java.io.StringBufferInputStream;
import java.io.DataInputStream;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;
import com.netscape.jsdebugging.api.*;

// XXX deal with window closing -- remove as Observer

public class SourceView extends InternalWindow
    implements Observer
{

    public SourceView( Rect rect,
                       Emperor emperor,
                       SourceTextItem item )
    {
        super(rect);

        _item = item;

        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _sourceTyrant = emperor.getSourceTyrant();
        _breakpointTyrant = emperor.getBreakpointTyrant();
        _sourceViewManager = emperor.getSourceViewManager();
        _stackTyrant    = emperor.getStackTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_breakpointTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceViewManager,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant,"emperor init order problem", this);

        _sourceLineVectorModel =
            new SourceLineVectorModel(this, _controlTyrant, _stackTyrant,
                                      _sourceTyrant, item);
        _listview = new SourceTextListView(this);

        Rect rectSG = new Rect(rect);
        rectSG.moveBy(-rect.x, -rect.y);
        ScrollGroup sg = new ScrollGroup(rectSG);
        sg.setHorizScrollBarDisplay( ScrollGroup.ALWAYS_DISPLAY );
        sg.setVertScrollBarDisplay(  ScrollGroup.ALWAYS_DISPLAY );
        sg.setContentView( _listview );
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        sg.setBackgroundColor(_emperor.getBackgroundColor());

        setCloseable( true );
        setResizable( true );
//        setTitle( "Source for: " + item.getURL() );
        setTitle( item.getURL() );
        addSubview(sg);
        setAutoResizeSubviews(true);
        contentView().setLayoutManager( new MarginLayout() );

        _listview.setAllowsEmptySelection(true);
        _listview.setAllowsMultipleSelection(false);

        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        _sourceTyrant.addObserver(this);
        _controlTyrant.addObserver(this);
        _breakpointTyrant.addObserver(this);

        _refresh(true);

        layoutView(1,1);
        layoutView(-1,-1);

        show();
    }

    public void ensureSourceLineVisible( int line )
    {
        int userline = _sourceTyrant.systemLine2UserLine(_item,line);
        if(userline >= 1 && userline <= _listview.count())
            _listview.scrollItemAtToVisible(userline-1);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if( o == _sourceTyrant )
        {
            SourceTyrantUpdate update = (SourceTyrantUpdate) arg;
            switch( update.type )
            {
                case SourceTyrantUpdate.VECTOR_CHANGED:
                    _refresh(false);
                    updateMarks();
                    break;
                case SourceTyrantUpdate.ITEM_CHANGED:
                    if( update.item == _item )
                    {
                        _refresh(false);
                        updateMarks();
                    }
                    break;
                case SourceTyrantUpdate.ADJUSTMENTS_CHANGED:
                    updateMarks();
                    break;
                default:
                    break;  // ignore any other types
            }
        }
        else if ( o == _breakpointTyrant )
        {
            BreakpointTyrantUpdate update = (BreakpointTyrantUpdate) arg;
            switch( update.type )
            {
                case BreakpointTyrantUpdate.ADD_ONE:
                case BreakpointTyrantUpdate.REMOVE_ONE:
                case BreakpointTyrantUpdate.MODIFIED_ONE:
                    if( update.bp.getURL().equals(_item.getURL()) )
                        updateMarks();
//                        _refreshMarksForSingleLine(update.bp.getLine());
                    break;
                case BreakpointTyrantUpdate.REMOVE_ALL_FOR_URL:
                    if( ! (update.bp.getURL().equals(_item.getURL())) )
                        break;
                    // else... FALL THROUGH
                case BreakpointTyrantUpdate.REMOVE_ALL:
                case BreakpointTyrantUpdate.REFRESH_ALL:
                    updateMarks();
                    break;
                default:
                    break;  // ignore any other types
            }
        }
        else if ( o == _controlTyrant )
        {
            // update all dirty views on stop
            if( ControlTyrant.STOPPED == _controlTyrant.getState() )
                _refresh(false);
            updateMarks();
        }

    }

    public void forceRefresh()
    {
        _refresh(true);
    }

    public void updateMarks()
    {
        _sourceLineVectorModel.updateLineItemVector();
        _listview.drawMarks();
    }

/*
    private synchronized void _refreshSingleLine( int line, Breakpoint bp )
    {
        if( null == _sourceLineVectorModel || null == _listview )
            return;
        _sourceLineVectorModel.updateSingleLineItem(line);
        // XXX This will change with 'selective text'
        _listview.drawItemAt(line-1);
    }
    private synchronized void _refreshMarksForSingleLine(int line)
    {
        if( null == _sourceLineVectorModel || null == _listview )
            return;
        _sourceLineVectorModel.updateSingleLineItem(line);
        // XXX This will change with 'selective text'
        _listview.drawMarksOfItemAt(line-1);
    }
*/

    private synchronized void _refresh(boolean force)
    {
        if( _item.getDirty() || force )
        {
            _emperor.setWaitCursor(true);

//            System.out.println(" _refreshing: " + _item.getURL() +
//                               " _item.getDirty() returned: " +
//                               (_item.getDirty() ? "true" : "false") +
//                               "_item.getAlterCount() returned: " +
//                               _item.getAlterCount() +
//                               " force == " +
//                               (force ? "true" : "false") );
            // refill all
            _listview.removeAllItems();
            _sourceLineVectorModel.rebuildLineItemVector();
            setSelection(null,0);

            Vector vec = _sourceLineVectorModel.getLineItemVector();
//            System.out.println( "vec size = " + vec.size() );

            Font linefont = _emperor.getFixedFont();

            boolean showNumbers = _sourceViewManager.getShowLineNumbers();
            SourceTextItemDrawer drawer = new SourceTextItemDrawer();

            _listview.setItemDrawer(drawer);

            int maxwidth = 0;
            int itemHeight = 0;
            for (int i = 0; i < vec.size(); i++)
            {
                SourceLineItemModel lineitem = (SourceLineItemModel) vec.elementAt(i);
                String text = lineitem.text;

                SourceTextListItem newlistitem = new SourceTextListItem(drawer,text.length());
                newlistitem.setTitle( text );
                newlistitem.setData( lineitem );
                newlistitem.setFont( linefont );
                newlistitem.setSelectedColor(_emperor.getSelectionColor());

                _listview.addItem( newlistitem );

                if( 0 == i )
                {
                    itemHeight = _listview.rectForItem(newlistitem).height;
                    drawer.init( linefont, showNumbers, vec.size(), itemHeight );
                }
                maxwidth = Math.max( maxwidth, newlistitem.minWidth() );
            }

            _listview.setBounds(0,0,maxwidth,0);
            _listview.sizeToMinSize();

            layoutView(0,0);
            _listview.draw();

            _item.setDirty(false);

            JSSourceLocation loc = _stackTyrant.getCurrentLocation();
            if( null != loc && loc.getURL().equals(getURL()) )
                ensureSourceLineVisible( loc.getLine() );

            _emperor.setWaitCursor(false);
        }
    }

    public String   getSelectedText() {return _selectedText;}
    public int      getSelectedLineNumber() {return _selectedLineNumber;}
    public void     setSelection(String s, int lineNumber)
    {
        if( _selectedLineNumber == lineNumber && s != null && s.equals(_selectedText) )
            return;
        _selectedText = s;
        _selectedLineNumber = lineNumber;
        _sourceViewManager.selectionInSourceViewChanged(this,s,lineNumber);
    }

    public SourceViewManager getSourceViewManager() {return _sourceViewManager;}
    public BreakpointTyrant  getBreakpointTyrant() {return _breakpointTyrant;}
    public Emperor           getEmperor() {return _emperor;}
    public SourceTyrant      getSourceTyrant() {return _sourceTyrant;}

    public SourceTextItem   getSourceItem() {return _item;}
    public String           getURL() {return _item.getURL();}

    private Emperor                 _emperor;
    private ControlTyrant           _controlTyrant;
    private SourceTyrant            _sourceTyrant;
    private StackTyrant             _stackTyrant;
    private BreakpointTyrant        _breakpointTyrant;
    private SourceViewManager       _sourceViewManager;
    private SourceLineVectorModel   _sourceLineVectorModel;
    private SourceTextItem          _item;
    private SourceTextListView      _listview;
    private String                  _selectedText = null;
    private int                     _selectedLineNumber = 0;


}

/***************************************************************************/

final class SourceTextListView extends SmartItemListView
{
    public SourceTextListView( SourceView sourceView )
    {
        super();
        _sourceView = sourceView;
    }


    public boolean mouseDown(MouseEvent me)
    {
        _dragOrigin = -1;

        // middle click
        if( me.isAltKeyDown() && _isInMarksZone(me) )
        {
            SourceTyrant st = getSourceView().getSourceTyrant();
            SourceTextItem sti =  _sourceView.getSourceItem();
            st.clearAllAdjustmentsForItem(sti);
            return false;
        }

        // right click
        if( me.isMetaKeyDown() && _isInMarksZone(me) )
        {
            SourceTyrant sourceTyrant = getSourceView().getSourceTyrant();

            _dragLast = _dragOrigin = _itemNumberAtPoint(me);
            SourceTextItem sti =  _sourceView.getSourceItem();
            int sysline = sourceTyrant.userLine2SystemLine(sti,_dragOrigin);
            _dragAdjLineOrigin = sysline;
//            System.out.println( "original line = " + _dragOrigin );
//            System.out.println( "sys line = " + sysline );
//            System.out.println( "\n" );
            return true;
        }
        return super.mouseDown(me);
    }


    public void mouseDragged(MouseEvent me)
    {
        if( -1 != _dragOrigin )
        {
            int curIndex = _itemNumberAtPoint(me);
            if( _dragLast == curIndex )
                return;
            _extendAdjLine(me);
        }
        else
            super.mouseDragged(me);
    }
    public void mouseUp(MouseEvent me)
    {
        if( -1 != _dragOrigin )
        {
            _extendAdjLine(me);
            _dragAdjLineOrigin = -1;
        }
        else
            super.mouseUp(me);
    }

    private final boolean _isInMarksZone(MouseEvent me)
    {
        return me.x < _drawer.marksWidth();
    }
    private final int _itemNumberAtPoint(MouseEvent me)
    {
        return 1 + indexOfItem(itemForPoint(me.x,me.y));
    }

    private void _extendAdjLine(MouseEvent me)
    {
        int curIndex = _itemNumberAtPoint(me);
        if( _dragLast == curIndex )
            return;
        int offset = curIndex - _dragLast;
        _dragLast = curIndex;

        SourceTyrant st = getSourceView().getSourceTyrant();
        SourceTextItem sti =  _sourceView.getSourceItem();
        int old_offset = st.getAdjustment(sti,_dragAdjLineOrigin);
        st.makeAdjustment(sti, _dragAdjLineOrigin, old_offset + offset);
    }

    protected void ancestorWillRemoveFromViewHierarchy(View removedView)
    {
        if( removedView == _sourceView )
        {
            _sourceView.getSourceViewManager().viewBeingRemoved(_sourceView);
        }

        super.ancestorWillRemoveFromViewHierarchy(removedView);
    }

    public void setItemDrawer(SourceTextItemDrawer drawer) {_drawer = drawer;}

    public void drawMarks()
    {
        if(AS.S)ER.T(null!=_drawer,"SourceTextListView has null drawer",this);
        if( null == _drawer || null == _drawer.marksRect())
        {
            draw();
            return;
        }
        // calc marks rect and call draw()
        Rect r = new Rect( 0,0, _drawer.marksRect().width, height() );
//        System.out.println( "draw marks rect: " + r );
        draw(r);
    }

    public void drawMarksOfItemAt(int i)
    {
        if(AS.S)ER.T(null!=_drawer,"SourceTextListView has null drawer",this);
        if( null == _drawer )
        {
            drawItemAt(i);
            return;
        }
        // calc marks rect for item and call draw()
        Rect rItem = rectForItemAt(i);
        Rect r = new Rect( rItem.x,
                           rItem.y,
                           _drawer.marksRect().width,
                           rItem.height );
//        System.out.println( "draw marks rect for single item: " + r );
        draw(r);
    }

    public SourceTextListItem getItemWithSel() {return _itemWithSel;}
    public int getSelStart()                   {return _selStart;}
    public int getSelEnd()                     {return _selEnd;}
    public void setItemWithSel(SourceTextListItem i) {_itemWithSel=i;}
    public void setSelRange(int start, int end) {_selStart=start;_selEnd=end;}

    public void selChangeCompleted()
    {
        int lineNumber = 0;
        _selString = null;

        if( null != _itemWithSel )
        {
            _selString = _itemWithSel.stringInRange(_selStart,_selEnd);
            lineNumber = indexOfItem(_itemWithSel) + 1;
        }
        _sourceView.setSelection(_selString, lineNumber );
    }

    public SourceView getSourceView() {return _sourceView;}

    private SourceView           _sourceView;
    private SourceTextItemDrawer _drawer;
    private SourceTextListItem   _itemWithSel;
    private int                  _selStart;
    private int                  _selEnd;
    private String               _selString;
    private int                  _dragOrigin = -1;
    private int                  _dragLast;
    private int                  _dragAdjLineOrigin;

}

/***************************************************************************/

final class SourceTextItemDrawer
{
    public SourceTextItemDrawer() {}

    public void init( Font font, boolean showLineNumbers,
                      int lineCount, int itemHeight )
    {
        _font = font;
        _showLineNumbers = showLineNumbers;

        _lineNumberColumnCount = String.valueOf(lineCount).length();
//        _height = font.fontMetrics().stringHeight();
        _height = itemHeight;
        _charWidth = font.fontMetrics().charWidth('0');

        _rectBP       = new Rect( 0, 0,
                                  _height, _height);
        Rect rectExec = new Rect( _rectBP.x + _rectBP.width, 0,
                                  _height/2, _height);
        Rect rectNum = new Rect( rectExec.x  + rectExec.width, 0,
                                 _showLineNumbers ? _charWidth*(_lineNumberColumnCount+1) : 0, _height);

        _width = rectNum.x + rectNum.width;

        _ptLineNumbers = new Point(rectNum.x, rectNum.y);

        int margin = _height / 4;

        // size before shrink
        _rectExecPointBackground = new Rect( rectExec );
        _rectBPBackground        = new Rect( _rectBP );
        _rectMarks               = Rect.rectFromUnion(rectExec, _rectBP);
        if( null == _rectMarks )
            _rectMarks = new Rect();

        _rectBP.growBy( -margin, - margin );
        rectExec.growBy( -(margin/2), -margin );

//        _rectBP.moveBy( 0, -margin/2 );
//        rectExec.moveBy( 0, -margin/4 );

        _polyExecPoint  = new Polygon();
        _polyExecPoint.addPoint( rectExec.x, rectExec.y );
        _polyExecPoint.addPoint( rectExec.x, rectExec.y + rectExec.height);
        _polyExecPoint.addPoint( rectExec.x+rectExec.width, rectExec.midY());
        _polyExecPoint.addPoint( rectExec.x, rectExec.y );

        _ptText = new Point( _width, 0 );
        _stringClipper = new DrawStringClipper( _charWidth );
    }

    public void draw( Graphics g,
                      Rect boundsRect,
                      SourceTextListItem item,
                      int lineNumber,
                      boolean hasBreakpoint,
                      boolean hasConditionalBreakpoint,
                      int scriptType,
                      boolean isExecPoint,
                      String adjustmentChar  )
    {
        SourceLineItemModel itemModel = (SourceLineItemModel) item.data();

        Rect r = new Rect();
        Rect clipRect = g.clipRect();

        // draw the main text

        String text = item.title();
        if( null != text && 0 != text.length() )
        {
            r.setBounds( boundsRect.x + _ptText.x,
                         boundsRect.y + _ptText.y,
                         boundsRect.width - _ptText.x,
                         boundsRect.height - _ptText.y );
            
//            System.out.println("r is "+r+"  clip is "+clipRect);            
            if( r.intersects( clipRect ) )
            {
                /* no sync - we trust that we are drawn on only one thread */
                if( _stringClipper.doClip(text, r, clipRect) > 0 )
                    item.drawStringInRect(g, _stringClipper.getString(), _font,
                                          _stringClipper.getRect(),
                                          Graphics.LEFT_JUSTIFIED);
            }
        }

        if( hasBreakpoint )
        {
            r.setBounds( boundsRect.x + _rectBP.x,
                         boundsRect.y + _rectBP.y,
                         _rectBP.width,
                         _rectBP.height );

            if( r.intersects( clipRect ) )
            {
                if( hasConditionalBreakpoint )
                    g.setColor( Color.orange );
                else
                    g.setColor( Color.red );
                g.fillOval( r );
            }
        }

        if( null != adjustmentChar )
        {
            r.setBounds( boundsRect.x + _rectBPBackground.x,
                         boundsRect.y + _rectBPBackground.y,
                         _rectBPBackground.width,
                         _rectBPBackground.height );

            if( r.intersects( clipRect ) )
            {
                g.setColor( Color.black );
                g.setFont(_font);
                g.drawStringInRect(adjustmentChar,r,Graphics.LEFT_JUSTIFIED);
            }
        }

        if( scriptType != SourceLineItemModel.NO_SCRIPT )
        {
            r.setBounds( boundsRect.x + _rectExecPointBackground.x,
                         boundsRect.y + _rectExecPointBackground.y,
                         _rectExecPointBackground.width,
                         _rectExecPointBackground.height );

            if( r.intersects( clipRect ) )
            {
                if( scriptType == SourceLineItemModel.FUNCTION_BODY )
                    g.setColor( Color.orange );
                else
                    g.setColor( Color.yellow );

                g.fillRect( r );
            }
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
        if( _showLineNumbers )
        {
            r.setBounds( boundsRect.x + _ptLineNumbers.x,
                         boundsRect.y + _ptLineNumbers.y,
                         boundsRect.width - _ptLineNumbers.x,
                         boundsRect.height - _ptLineNumbers.y );

            if( r.intersects( clipRect ) )
            {
                g.setColor( Color.darkGray );
                g.setFont(_font);
                g.drawStringInRect(lineNumberString(lineNumber),r,Graphics.LEFT_JUSTIFIED);
            }
        }
    }

    public int width() {return _width;}
    public int height() {return _height;}
    public int charWidth() {return _charWidth;}
    public Rect breakpointRect() {return _rectBPBackground;}
    public Rect marksRect()      {return _rectMarks;}
    public Point textOrigin() {return _ptText;}
    public int marksWidth() {return _rectMarks.width;}

    private String lineNumberString( int number )
    {
        StringBuffer buf = new StringBuffer();
        String s = String.valueOf(number);
        int len = s.length();
        if(AS.S)ER.T(len<=_lineNumberColumnCount,"lineNumberColumnCount screwed up",this);

        for( int i = 0; i < _lineNumberColumnCount-len; i++ )
            buf.append('0');
        buf.append(s);
        buf.append(':');

        return buf.toString();
    }

    private int _width;
    private int _height;

    private int _lineNumberColumnCount;
    private Font _font;

    private Rect    _rectBP;
    private Rect    _rectBPBackground;
    private Polygon _polyExecPoint;
    private Rect    _rectExecPointBackground;
    private Rect    _rectMarks;
    private Point   _ptLineNumbers;
    private Point   _ptText;
    private boolean _showLineNumbers;
    private int     _charWidth;
    private DrawStringClipper _stringClipper;


}

final class SourceTextListItem extends SmartListItem
{
    public SourceTextListItem(SourceTextItemDrawer drawer, int charCount)
    {
        super();
        _drawer = drawer;
        _charCount = charCount;
    }

    public boolean mouseDown(MouseEvent me)
    {
/*
        System.out.println(".......................");
        System.out.println("mousedown x=" + me.x + " y=" + me.y);
        System.out.println(myRect());
        System.out.println(".......................");
*/

        _dragOrigin = -1;

        // middle click
        if( me.isAltKeyDown() )
            return false;

        // right click
        if( me.isMetaKeyDown() )
        {
            // evaluate selected text
            SourceTextListView v = (SourceTextListView) listView();
            if( v.getItemWithSel() != this )
                return false;

            Rect rectText = textRect();
            if( ! rectText.contains(me.x,me.y) )
                return false;

            Emperor emperor = v.getSourceView().getEmperor();
            CommandTyrant ct = emperor.getCommandTyrant();
            Menu menu = new Menu(true);

            if( ControlTyrant.STOPPED == emperor.getControlTyrant().getState() )
            {
                menu.addItem("Eval", ct.cmdString(CommandTyrant.EVAL_SEL_STRING), ct);
                menu.addItem("Inspect...", ct.cmdString(CommandTyrant.INSPECT_SEL_STRING), ct);
                menu.addItem("Copy to Watch", ct.cmdString(CommandTyrant.COPY_TO_WATCH), ct);
                menu.addSeparator();
            }

            menu.addItem("Copy", ct.cmdString(CommandTyrant.COPY), ct);
            menu.addSeparator();

            CmdState cs = ct.findCmdState(CommandTyrant.TOGGLE_BREAKPOINT);
            if( null != cs && cs.enabled )
            {
                if( cs.checked )
                {
                    menu.addItem("Clear Breakpoint", ct.cmdString(CommandTyrant.TOGGLE_BREAKPOINT), ct);
                    menu.addItem("Edit Breakpoint...", ct.cmdString(CommandTyrant.EDIT_BREAKPOINT), ct);
                }
                else
                    menu.addItem("Set Breakpoint", ct.cmdString(CommandTyrant.TOGGLE_BREAKPOINT), ct);
            }

//            String cmd = ct.cmdString( CommandTyrant.EVAL_STRING );
//            String str = stringInRange(v.getSelStart(), v.getSelEnd());
//            Application.application().performCommandLater( ct, cmd, str );

            ListView lv = listView();
            RootView rv = lv.rootView();
            MouseEvent tme = lv.convertEventToView(rv, me);
            MouseMenuView mmv = new MouseMenuView(menu);
            mmv.show(rv, tme);

            return false;
        }

        Rect rectBP = breakpointRect();
        if( rectBP.contains(me.x,me.y) )
        {
            // toggle breakpoint
            SourceLineItemModel model = (SourceLineItemModel) data();
            SourceView sv = ((SourceTextListView)listView()).getSourceView();
            BreakpointTyrant bpt = sv.getBreakpointTyrant();
            SourceTyrant sourceTyrant = sv.getSourceTyrant();
            SourceTextItem sti =  sv.getSourceItem();

            int sysline = sourceTyrant.userLine2SystemLine(sti,model.lineNumber);

            Location loc = new Location( sv.getURL(), sysline );

            Breakpoint bp = bpt.findBreakpoint(loc);
            if( null == bp )
                bpt.addBreakpoint(loc);
            else
                bpt.removeBreakpoint(bp);

            // redraw done on notification...
        }
        else
        {
            // start select (or select word)
            Rect rectText = textRect();
            if( rectText.contains(me.x,me.y) )
            {
                int start, end;
                SourceTextListView v = (SourceTextListView) listView();
                start = end = charIndexAtPointInRect( me.x, rectText );

                SourceTextListItem oldItem = v.getItemWithSel();
                v.setItemWithSel(this);

                if( me.clickCount() > 1 )
                {
                    Range r = wordAtIndex(start);
                    if( null != r )
                    {
                        start = r.index;
                        end = start + r.length-1;
                    }
                    _dragOrigin = -1;
                }
                else
                    _dragOrigin = start;

                v.setSelRange(start, end);

                if( null != oldItem )
                    v.drawItemAt( v.indexOfItem(oldItem) );
                v.draw( rectOfRangeInRect(start, end, rectText) );

                if( _dragOrigin == -1 ) // i.e. was double click
                    v.selChangeCompleted();
            }
        }
        return true;
    }

    public void mouseDragged(MouseEvent me)
    {
        SourceTextListView v = (SourceTextListView) listView();
        if( -1 != _dragOrigin && v.getItemWithSel() == this )
            _extendSelection(me, v);
    }

    public void mouseUp(MouseEvent me)
    {
        SourceTextListView v = (SourceTextListView) listView();
        if( -1 != _dragOrigin && v.getItemWithSel() == this )
        {
            _extendSelection(me, v);
            v.selChangeCompleted();
            _dragOrigin = -1;
        }
    }

    private void _extendSelection(MouseEvent me, SourceTextListView v)
    {
        Rect rectText = textRect();
        if( rectText.contains(me.x,me.y) )
        {
            int cur = charIndexAtPointInRect( me.x, rectText );
            int oldStart = v.getSelStart();
            int oldEnd   = v.getSelEnd();

            int start;
            int end;

            if( cur > _dragOrigin )
            {
                start = _dragOrigin;
                end   = cur;
            }
            else
            {
                start = cur;
                end   = _dragOrigin;
            }
            if( start == oldStart && end == oldEnd )
                return;

            v.setSelRange(start, end);

            // redraw only changed parts (not worried about overlap)...
            int x1, x2;
            x1 = Math.min(oldStart, start);
            x2 = Math.max(oldStart, start);
            if( x1 != x2 )
                v.draw( rectOfRangeInRect(x1, x2, rectText) );
            x1 = Math.min(oldEnd, end);
            x2 = Math.max(oldEnd, end);
            if( x1 != x2 )
                v.draw( rectOfRangeInRect(x1, x2, rectText) );
        }
    }

    public int minWidth() {return super.minWidth() + _drawer.width();}


    public Range wordAtIndex(int index)
    {
        String text = ((SourceLineItemModel)data()).text;
        int len = text.length();
        int start = -1;
        int end;
        int i;

        // search for first char...
        for( i = 0; i <= index; i++ )
        {
            char c = text.charAt(i);
            if( -1 == start )
            {
                if( Character.isJavaIdentifierStart(c) )
                    start = i;
            }
            else
            {
                if( ! Character.isJavaIdentifierPart(c) )
                    start = -1;
            }
        }
        if( -1 == start )
            return null;

        for(; i < len; i++ )
        {
            char c = text.charAt(i);
            if( ! Character.isJavaIdentifierPart(c) )
                break;
        }

        return new Range(start, i-start);
    }

    public String stringInRange(int start, int end)
    {
        return ((SourceLineItemModel)data()).text.substring(start,end+1);
    }



    protected void drawBackground(Graphics g, Rect boundsRect)
    {
        SourceTextListView v = (SourceTextListView) listView();

        if( v.getItemWithSel() == this )
        {
            Rect r = rectOfRangeInRect(v.getSelStart(),v.getSelEnd(),textRect());
            r.intersectWith(boundsRect);
            if( ! r.isEmpty() )
            {
                g.setColor(selectedColor());
                g.fillRect(r);
            }
        }
    }

    public void drawInRect(Graphics g, Rect boundsRect)
    {
        drawBackground(g, boundsRect);

        SourceLineItemModel model = (SourceLineItemModel) data();
        _drawer.draw( g, boundsRect, this,
                      model.lineNumber,
                      model.bp != null,
                      model.bp != null?model.bp.getBreakCondition()!=null:false,
                      model.type,
                      model.executing,
                      model.adjustmentChar );
    }

    // make this public to be called by our drawer helper
    public void drawStringInRect( Graphics g, String title,
                                  Font titleFont, Rect textBounds,
                                  int justification)
    {
        // XXX temp test...
//        if(AS.DEBUG) {
//            int len = title.length();
//            String sub;
//            if(0 == len)
//                sub = "BLANK";
//            else
//                sub = title.substring(0, Math.min(60,len));
//            System.out.println("drawing text: \""+sub+"\" at "+textBounds);
//        }
        super.drawStringInRect(g,title,titleFont,textBounds,justification);
    }

    public Rect breakpointRect()
    {
//        Rect r = new Rect( _drawer.breakpointRect() );
        Rect r = new Rect( _drawer.marksRect() );
        Rect myRect = myRect();
        r.moveBy(myRect.x, myRect.y);
        return r;
    }

    public Rect textRect()
    {
        Rect r = new Rect( _drawer.textOrigin().x,
                           _drawer.textOrigin().y,
                           _drawer.charWidth() * _charCount,
                           _drawer.height() );
        Rect myRect = myRect();
        r.moveBy(myRect.x, myRect.y);
        return r;
    }

    // ASSUMES that point is in rect -- caller beware!
    protected int charIndexAtPointInRect( int x, Rect r )
    {
        return (x - r.x) / _drawer.charWidth();
    }

    protected Rect rectOfRangeInRect( int start, int end, Rect r )
    {
        int cw = _drawer.charWidth();
        return new Rect((start*cw)+r.x, r.y, (1+end-start)*cw, r.height);
    }

    private int                  _dragOrigin = -1;
    private int                  _charCount;
    private SourceTextItemDrawer _drawer;
}

/***************************************************************************/

final class SourceLineVectorModel
{
    public SourceLineVectorModel( SourceView sourceView,
                                  ControlTyrant controlTyrant,
                                  StackTyrant   stackTyrant,
                                  SourceTyrant  sourceTyrant,
                                  SourceTextItem sourceTextItem )
    {
        _sourceView     = sourceView;
        _controlTyrant  = controlTyrant;
        _stackTyrant    = stackTyrant;
        _sourceTextItem = sourceTextItem;
        _sourceTyrant   = sourceTyrant;
        _sourceLineItemVector = new Vector();
    }

    public synchronized void rebuildLineItemVector()
    {
        Vector  vec = new Vector();
        try
        {
            DataInputStream s = new DataInputStream(
                                        new StringBufferInputStream(
                                                getSourceTextItem().getText() ));
            if( null != s )
            {
                String linetext;
                int lineNumber = 1;
                while( null != (linetext = s.readLine()) )
                {
                    SourceLineItemModel item = new SourceLineItemModel();

                    item.lineNumber       = lineNumber;
                    item.text             = Util.expandTabs(linetext,8);
                    item.type             = SourceLineItemModel.NO_SCRIPT;
                    item.bp               = null;   // XXX
                    item.executing        = false;
                    item.adjustmentChar   = null;

                    vec.addElement( item );
                    lineNumber++ ;
                }
                s.close();
            }
        }
        catch( Exception e ) {}

        _sourceLineItemVector = vec;
        updateLineItemVector();
    }

    public synchronized void updateLineItemVector()
    {
/*
        // XXX script stuff must change...
        ScriptProvider sp = JSDScriptProvider.getScriptProvider();
        if( null == sp )
            return;
*/

        int i;
        int count;

        // clear all line

        int maxLineItemIndex = _sourceLineItemVector.size() - 1;

        count = _sourceLineItemVector.size();
        if( 0 == count )
            return;
        for( i = 0; i < count; i++ )
        {
            SourceLineItemModel item =
                        (SourceLineItemModel) _sourceLineItemVector.elementAt(i);
            item.type       = SourceLineItemModel.NO_SCRIPT;
            item.bp         = null;
            item.executing  = false;
            item.adjustmentChar = null;
        }

        // walk through the scripts and mark lines with scripts

        String url = _sourceTextItem.getURL();
        BreakpointTyrant bpt = _sourceView.getBreakpointTyrant();

        Vector vecScripts = bpt.getScriptsVectorForURL(url);
        count = vecScripts.size();
        for( i = 0; i < count; i++ )
        {
            Script script = (Script) vecScripts.elementAt(i);

            if( ! script.isValid() )
                continue;

            boolean fun = null != script.getFunction();

            ScriptSection[] sections = script.getSections();
            for( int n = 0; n < sections.length; n++ )
            {
                int base   = sections[n].getBaseLineNumber();
                int extent = sections[n].getLineExtent();

                for( int k = base; k < (base+extent); k++ )
                {
                    if( k > maxLineItemIndex+1 )
                        continue;
                    int j = _sourceTyrant.systemLine2UserLine(_sourceTextItem,k);
                    if( j > maxLineItemIndex+1 )
                        continue;
                    SourceLineItemModel item =
                            (SourceLineItemModel) _sourceLineItemVector.elementAt(j-1);

                    // don't overwrite function with top level script

                    if( fun )
                        item.type = SourceLineItemModel.FUNCTION_BODY;
                    else if( item.type != SourceLineItemModel.FUNCTION_BODY )
                        item.type = SourceLineItemModel.TOP_LEVEL_SCRIPT;
                }
            }

        }

        // set breakpoints
        Vector vecBP = bpt.getBreakpointsForURL( _sourceTextItem.getURL() );
        count = vecBP.size();
        for( i = 0; i < count; i++ )
        {
            Breakpoint bp = (Breakpoint)vecBP.elementAt(i);
            int line = bp.getLine();
            line = _sourceTyrant.systemLine2UserLine(_sourceTextItem,line);

            if( line > maxLineItemIndex+1 )
                continue;

            SourceLineItemModel item = (SourceLineItemModel)
                            _sourceLineItemVector.elementAt(line-1);
            item.bp = bp;
        }

        // set executing lines

        if( ControlTyrant.STOPPED ==  _controlTyrant.getState() )
        {
            JSSourceLocation loc = _stackTyrant.getCurrentLocation();
            if( null != loc && loc.getURL().equals(url) )
            {
                int index = loc.getLine();
                index = _sourceTyrant.systemLine2UserLine(_sourceTextItem,index);

                if( index > maxLineItemIndex+1 )
                    index = maxLineItemIndex+1;
                if( index < 1 )
                    index = 1;

                SourceLineItemModel item = (SourceLineItemModel)
                               _sourceLineItemVector.elementAt(index-1);
                item.executing  = true;
            }
        }

        // set adjustment chars

        int[] adjArray = _sourceTyrant.getUserAdjustedLineArray(_sourceTextItem);
        if( null != adjArray )
        {
            String adjChar;
            for( int k = 0; k < adjArray.length; k++ )
            {
                int line = adjArray[k];
                if( line < 0 )
                {
                    line *= -1;
                    adjChar = "-";
                }
                else
                    adjChar = "+";
                SourceLineItemModel item = (SourceLineItemModel)
                               _sourceLineItemVector.elementAt(line-1);
                item.adjustmentChar = adjChar;
            }
        }
    }


    // This is out of date. If anyone wants to reserect it then they
    // had better update this to do the right things!
/*
    public synchronized void updateSingleLineItem( int lineNumber )
    {
        BreakpointTyrant bpt = _sourceView.getBreakpointTyrant();
        Breakpoint bp = bpt.findBreakpoint(
                            new Location(_sourceTextItem.getURL(),lineNumber));

        int maxLineItemIndex = _sourceLineItemVector.size() - 1;
        if( lineNumber > maxLineItemIndex+1 )
            lineNumber = maxLineItemIndex+1;

        SourceLineItemModel item = (SourceLineItemModel)
                        _sourceLineItemVector.elementAt(lineNumber-1);
        item.bp = bp;
    }
*/

    public Vector            getLineItemVector()    {return _sourceLineItemVector;}
    public SourceTextItem    getSourceTextItem()    {return _sourceTextItem;}

    private SourceView      _sourceView;
    private SourceTyrant    _sourceTyrant;
    private ControlTyrant   _controlTyrant;
    private StackTyrant     _stackTyrant;
    private SourceTextItem  _sourceTextItem;    // has url, text, and fullness status
    private Vector          _sourceLineItemVector;
}



