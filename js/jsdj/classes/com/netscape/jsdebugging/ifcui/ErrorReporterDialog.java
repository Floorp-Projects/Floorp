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
* Dialog to display error info and get user's response
*/

// when     who     what
// 06/30/97 jband   added this class
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.api.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

class ErrorReporterDialog
    extends InternalWindow
    implements Target
{
    private static final int _boxDX     = 350;
    private static final int _boxDY     = 150;
    private static final int _buttonDX  = 64;
    private static final int _buttonDY  = 24;
    private static final int _spacerDX  = 5;
    private static final int _spacerDY  = 5;

    public ErrorReporterDialog( Emperor emperor, ErrorReport er )
    {
        super();

        setTitle("JavaScript Error");
        setCloseable( false );
        setResizable( false );

        Button icon = new Button(_spacerDX,_spacerDY,100,100);
        icon.setEnabled(false);
        icon.setBordered(false);
        icon.setImage(Alert.warningImage());
        Size sz = icon.minSize();
        icon.sizeTo(sz.width,sz.height);
        addSubview(icon);

        int boxTop = _spacerDY + sz.height + _spacerDY;

        int contentDX = _boxDX + _spacerDX * 2;
        int contentDY = boxTop + _boxDY + _spacerDY * 3 + _buttonDY;
        int buttonY   = boxTop + _boxDY + _spacerDY * 2;
        int buttonX3  = _boxDX + _spacerDX - _buttonDX;
        int buttonX2  = buttonX3 - _spacerDX - _buttonDX;
        int buttonX1  = buttonX2 - _spacerDX - _buttonDX;

        Size size = windowSizeForContentSize(contentDX, contentDY);
        setBounds(0,0,size.width,size.height);

        int titleX = _spacerDX + sz.width + _spacerDX;
        TextField title = new TextField(titleX, _spacerDY,
                                        contentDX-titleX, sz.height);
        title.setFont(new Font(Font.defaultFont().name(), Font.BOLD, 18));
        title.setJustification(Graphics.LEFT_JUSTIFIED);
        title.setStringValue("JavaScript Error");
        title.setBorder(null);
        title.setBackgroundColor(Color.lightGray);
        title.setEditable(false);
        addSubview(title);

        ERDListView lv = new ERDListView( emperor.getFixedFont() );

        lv.setAllowsEmptySelection(true);
        lv.setAllowsMultipleSelection(false);
        lv.setBackgroundColor(Color.lightGray);

        if( null != er.filename )
        {
            lv.addLine( "IN "+ er.filename );
            lv.addLine( "LINE "+ er.lineno );
            lv.addLine( "" );
        }

        lv.addLine( er.msg );

        if( null != er.linebuf )
        {
            StringBuffer sb = new StringBuffer(er.tokenOffset+2);
            for(int i = 0; i < er.tokenOffset; i++ )
                sb.append('.');
            sb.append('^');

            lv.addLine( "" );
            lv.addLine( er.linebuf );
            lv.addLine( sb.toString() );
        }
        lv.sizeToContent();

        ScrollGroup sg1 = new ScrollGroup(_spacerDX,boxTop,_boxDX,_boxDY);
        sg1.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg1.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg1.setContentView( lv );
        sg1.setAutoResizeSubviews(true);
        sg1.contentView().setLayoutManager( new MarginLayout() );
        sg1.setBackgroundColor(Color.lightGray);
        addSubview(sg1);

        Button button;

        button = new Button(buttonX1,buttonY,_buttonDX,_buttonDY);
        button.setTitle("OK");
        button.setTarget(this);
        button.setCommand(OK_CMD);
        addSubview(button);

        button = new Button(buttonX2,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Debug");
        button.setTarget(this);
        button.setCommand(DEBUG_CMD);
        if( "syntax error".equals(er.msg) )
            button.setEnabled(false);
        addSubview(button);

        button = new Button(buttonX3,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Pass On");
        button.setTarget(this);
        button.setCommand(PASS_ON_CMD);
        addSubview(button);

        center();
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(OK_CMD) )
            _answer = JSErrorReporter.RETURN;
        else if( cmd.equals(DEBUG_CMD) )
            _answer = JSErrorReporter.DEBUG;
        else if( cmd.equals(PASS_ON_CMD) )
            _answer = JSErrorReporter.PASS_ALONG;
        else
            return;
        hide();
    }

    public int  getAnswer() {return _answer;}

    private int       _answer = JSErrorReporter.RETURN;

    private static final String OK_CMD      = "OK_CMD";
    private static final String DEBUG_CMD   = "DEBUG_CMD";
    private static final String PASS_ON_CMD = "PASS_ON_CMD";
}

class ERDListView extends BackgroundHackListView
{
    public ERDListView( Font font )
    {
        super();
        _linefont        = font;
    }

    public void addLine( String text )
    {
        ListItem item = new ListItem();
        item.setTitle( text );
        item.setFont( _linefont );
        addItem( item );

        _maxlinelen = Math.max( _maxlinelen, text.length() );
    }

    public void sizeToContent()
    {
        FontMetrics fm = _linefont.fontMetrics();
        setBounds( 0, 0, (_maxlinelen+1) * fm.charWidth('X'),0 );
        sizeToMinSize();
    }

    // don't allow selection...
    public boolean mouseDown(MouseEvent me) {return false;}
    public void mouseDragged(MouseEvent me) {}
    public void mouseUp(MouseEvent me)      {}

    private Font    _linefont = null;
    private int     _maxlinelen = 0;
}
