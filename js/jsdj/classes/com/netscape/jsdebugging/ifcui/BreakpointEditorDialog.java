/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
* Dialog UI for editing single breakpoint and its condition
*/

// when     who     what
// 10/14/97 jband   created class
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

class BreakpointEditorDialog
    extends InternalWindow
    implements Target
{
    private static final int _labelDX    = 55;
    private static final int _labelDY    = 24;
    private static final int _editDX     = 300;
    private static final int _editDY     = 24;
    private static final int _buttonDX   = 64;
    private static final int _buttonDY   = 24;
    private static final int _spacerDX  = 5;
    private static final int _spacerDY  = 5;

    public BreakpointEditorDialog(String title,
                                  int line,
                                  String url,
                                  String condition,
                                  Font font,
                                  boolean isNew )
    {
        super();

        setTitle(title);
        setCloseable( false );
        setResizable( false );

        int contentDX = _labelDX + _editDX + _spacerDX * 3;
        int contentDY = _editDY * 3 + _spacerDY * 5 + _buttonDY;
        int buttonY   = _editDY * 3 + _spacerDY * 4;
        int buttonX2  = contentDX - _spacerDX - _buttonDX;
        int buttonX1  = buttonX2 - _spacerDX - _buttonDX;
        int labelX    = _spacerDX;
        int editX     = labelX + _labelDX + _spacerDX;

        int Y1 = _spacerDY;
        int Y2 = Y1 + _editDY + _spacerDY;
        int Y3 = Y2 + _editDY + _spacerDY;

        Size size = windowSizeForContentSize(contentDX, contentDY);
        setBounds(0,0,size.width,size.height);

        addSubview( _newLabel(labelX, Y1, _labelDX, _labelDY, "line") );
        addSubview( _newLabel(labelX, Y2, _labelDX, _labelDY, "url") );
        addSubview( _newLabel(labelX, Y3, _labelDX, _labelDY, "condition") );

        _textFieldLine = new TextField(editX,Y1,_editDX,_editDY);
        _textFieldLine.setIntValue(line);
        _textFieldLine.setFont(font);
        addSubview(_textFieldLine);

        _textFieldURL = new TextField(editX,Y2,_editDX,_editDY);
        _textFieldURL.setStringValue(new String(url));
        _textFieldURL.setFont(font);
        addSubview(_textFieldURL);

        _textFieldCondition = new TextField(editX,Y3,_editDX,_editDY);
        _textFieldCondition.setStringValue(new String(condition));
        _textFieldCondition.setFont(font);
        addSubview(_textFieldCondition);

        Button button;

        button = new Button(buttonX1,buttonY,_buttonDX,_buttonDY);
        button.setTitle("OK");
        button.setTarget(this);
        button.setCommand(OK_CMD);
        addSubview(button);

        button = new Button(buttonX2,buttonY,_buttonDX,_buttonDY);
        button.setTitle("Cancel");
        button.setTarget(this);
        button.setCommand(CANCEL_CMD);
        addSubview(button);

        if( isNew )
            setFocusedView( _textFieldLine );
        else
            setFocusedView( _textFieldCondition );
        center();
    }

    private TextField _newLabel( int x, int y, int dx, int dy, String text )
    {
        TextField label = new TextField(x,y,dx,dy);
        label.setStringValue(text);
        label.setEditable(false);
        label.setSelectable(false);
        label.setJustification(Graphics.RIGHT_JUSTIFIED);
        label.setBackgroundColor(Color.lightGray);
        label.setBorder(null);
        return label;
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(OK_CMD) )
        {
            // get and validate data...

            _URL = _textFieldURL.stringValue();
            if( null == _URL || 0 == _URL.trim().length() )
            {
                Alert.runAlertInternally( Alert.notificationImage(),
                                          "invalid entry",
                                          "you must enter a valid url",
                                          "OK", null,null );
                setFocusedView( _textFieldURL );
                return;
            }

            _condition = _textFieldCondition.stringValue();
            if( null == _condition || 0 == _condition.trim().length() )
            {
                _condition = "true";
            }

            _line = 0;
            try
            {
                String str = _textFieldLine.stringValue();
                if( null != str &&  0 != str.trim().length() )
                    _line = Integer.parseInt(str);
            }
            catch( NumberFormatException e )
            {
                // eat it
            }
            if( _line <= 0 )
            {
                Alert.runAlertInternally( Alert.notificationImage(),
                                          "invalid entry",
                                          "line number must be >= 1",
                                          "OK", null,null );
                setFocusedView( _textFieldLine );
                return;
            }

            _ok = true;
            hide();
        }
        else if( cmd.equals(CANCEL_CMD) )
        {
            hide();
        }
    }

    public boolean okPressed()      {return _ok;}
    public String getURL()          {return _URL;}
    public String getCondition()    {return _condition;}
    public int getLine()            {return _line;}


    private TextField _textFieldLine;
    private TextField _textFieldURL;
    private TextField _textFieldCondition;
    private String    _URL;
    private int       _line;
    private String    _condition;
    private boolean   _ok = false;
    private static final String OK_CMD     = "OK_CMD";
    private static final String CANCEL_CMD = "CANCEL_CMD";
}


