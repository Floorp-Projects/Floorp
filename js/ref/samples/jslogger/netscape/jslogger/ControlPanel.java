/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package netscape.jslogger;

import netscape.application.*;
import netscape.util.*;

/**
*  Main entry point and workhorse for JavaScript Logger
*
* @author  John Bandhauer
*/

public class ControlPanel
    extends View
    implements Target, TextFilter
{
    public static ControlPanel create(JSLogger logger)
    {
        if( ! logger.isApplet() )
        {
            ExternalWindow _mainWindow = new ExternalWindow();
            _mainWindow.setBounds(0, 0, 500, 300);
            _mainWindow.setTitle("JSLogger");
            _mainWindow.setResizable(false);
            logger.setMainRootView(_mainWindow.rootView());
            _mainWindow.show();
        }
        logger.mainRootView().setColor(Color.lightGray);
//        logger.mainRootView().setColor(Color.white);
        return new ControlPanel(logger, logger.mainRootView().localBounds());
    }
    private static final int DX               = 500;

    private static final int CHECK_DX         = 160;
    private static final int CHECK_1_X        = 2;
    private static final int CHECK_2_X        = CHECK_1_X+CHECK_DX+2;
    private static final int CHECK_INITAL_Y   = 2;
    private static final int CHECK_DY         = 24;
    private static final int CHECK_SPACER_DY  = 0;
    private static final int CHECK_SUBCOL_DX  = 10;

    private static final int LABEL_X          = CHECK_1_X;
    private static final int LABEL_DX         = 160;
    private static final int EDIT_X           = LABEL_X + LABEL_DX + 2;
    private static final int EDIT_DX          = DX - (LABEL_DX+2+2);
    private static final int LABEL_EDIT_DY    = 24;
    private static final int LABEL_EDIT_SP_DY = 0;

    private int[] check_y = {CHECK_INITAL_Y, CHECK_INITAL_Y};
    private int[] check_x = {CHECK_1_X, CHECK_2_X};

    private int label_edit_y;

    private ControlPanel(JSLogger logger, Rect rect)
    {
        super(rect);
        _logger = logger;
        _logger.mainRootView().addSubview(this);

        _checkbox(0,0,"Logging On",         JSLogger.LOGGING_ON);
        _checkbox(0,1,"Log Date",           JSLogger.LOG_DATE);
        _checkbox(0,1,"Log Thread Name",    JSLogger.LOG_THREADNAME);
        _checkbox(0,1,"Log to File",        JSLogger.LOG_TO_FILE);
        _checkbox(0,1,"Log to Console",     JSLogger.LOG_TO_CONSOLE);
        _checkbox(1,0,"Log Execution",      JSLogger.TRACE_EXECUTION);
        _checkbox(1,1,"Log Function Calls", JSLogger.TRACE_FUNCTION_CALLS);
        _checkbox(1,2,"Log Stack Trace",    JSLogger.TRACE_DUMP_CALLSTACKS);
        _checkbox(1,1,"Log Each Line",      JSLogger.TRACE_EACH_LINE);
        _checkbox(1,0,"Log Script Loading", JSLogger.TRACE_SCRIPT_LOADING);

        label_edit_y = Math.max(check_y[0],check_y[1]) + 2;

        _edit("Eval Expression",            JSLogger.EVAL_EXPRESSION);
        _edit("Only Scripts From",          JSLogger.LOG_ONLY_SCRIPTS_FROM);
        _edit("Ignore Scripts From",        JSLogger.IGNORE_SCRIPTS_FROM);
        _edit("Only Functions Named",       JSLogger.LOG_ONLY_IN_FUNCTION);
        _edit("Ignore Functions Named",     JSLogger.IGNORE_IN_FUNCTION);
        _edit("Ignore Script Loading From", JSLogger.IGNORE_SCRIPT_LOADING_FROM);
    }

    private void _checkbox(int col, int subCol, String title, int cmdID)
    {
        Button button = 
            Button.createCheckButton(check_x[col] + CHECK_SUBCOL_DX * subCol, 
                                     check_y[col],
                                     CHECK_DX - CHECK_SUBCOL_DX * subCol, 
                                     CHECK_DY);
        button.setTitle(title);
        button.setTarget(this);
        button.setCommand(_makeBoolCmd(cmdID));
        button.setState(_logger.getBoolOption(cmdID));
        addSubview(button);

        check_y[col] += CHECK_DY + CHECK_SPACER_DY;
    }

    private void _edit(String title, int cmdID)
    {
        TextField label = new TextField(LABEL_X, label_edit_y,
                                        LABEL_DX, LABEL_EDIT_DY );
        label.setStringValue(title);
        label.setEditable(false);
        label.setSelectable(false);
        label.setJustification(Graphics.RIGHT_JUSTIFIED);
        label.setBackgroundColor(Color.lightGray);
        label.setBorder(null);
        addSubview(label);

        TextField edit = new TextField(EDIT_X, label_edit_y,
                                       EDIT_DX, LABEL_EDIT_DY );
        edit.setCommand(_makeStringCmd(cmdID));
        edit.setStringValue(_logger.getStringOption(cmdID));
        edit.setFilter(this); // to catch return key
        addSubview(edit);

        label_edit_y += LABEL_EDIT_DY + LABEL_EDIT_SP_DY;
    }

    // implement TextFilter interface
    public boolean acceptsEvent(Object o, KeyEvent ke , Vector vec)
    {
        if( ke.isReturnKey() )
        {
            try
            {
                TextField t = (TextField)o;
                Application.application().performCommandLater(this, t.command(), t);
                return false;
            }
            catch(Throwable t)
            {
                System.out.println(t);
                // eat it
            }
        }
        return true;
    }

    private String _tfs(Object data)
    {
        TextField t = (TextField)data;
        String s = t.stringValue();
        if( null == s || 0 == s.trim().length() )
            return null;
        return s;        
    }

    public void performCommand(String cmd, Object data)
    {
        if( _isBoolCmd(cmd) )
            _logger.setBoolOption(_getBoolCmdNumber(cmd),((Button)data).state());
        if( _isStringCmd(cmd) )
            _logger.setStringOption(_getStringCmdNumber(cmd),_tfs(data));
    }

    private String _makeBoolCmd(int num)    {return BOOL_CMD_PREFIX+num;}
    private String _makeStringCmd(int num)  {return STRING_CMD_PREFIX+num;}
    private boolean _isBoolCmd(String cmd)  {return cmd.startsWith(BOOL_CMD_PREFIX);}
    private boolean _isStringCmd(String cmd){return cmd.startsWith(STRING_CMD_PREFIX);}
    private int _getBoolCmdNumber(String cmd)
    {
        try
        {
            return Integer.parseInt(cmd.substring(BOOL_CMD_PREFIX.length()));
        }
        catch(Exception e)
        {
            // eat it...
        }
        return 0;
    }
    private int _getStringCmdNumber(String cmd)
    {
        try
        {
            return Integer.parseInt(cmd.substring(STRING_CMD_PREFIX.length()));
        }
        catch(Exception e)
        {
            // eat it...
        }
        return 0;
    }

    private static final String BOOL_CMD_PREFIX   = "BOOL_";
    private static final String STRING_CMD_PREFIX = "STRING_";

    private JSLogger _logger;
}        
