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
* Flexible Logging support
*/

// when     who     what
// 09/30/97 jband   created file
//

package com.netscape.jsdebugging.ifcui;

import java.io.*;
import java.util.Date;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

public final class Log
{
    public static final int ERROR    = 0;      // must start at 0 and be contiguous
    public static final int WARN     = 1;
    public static final int TRACE    = 2;
    public static final int LOG      = 3;
    public static final int TYPE_COUNT = 4; // count of above


    public static void error(String poster, String msg)
    {
        if( _enabled && _typeEnabled[ERROR] )
            _write( ERROR, poster, msg );
    }

    public static void warn(String poster, String msg)
    {
        if( _enabled && _typeEnabled[WARN] )
            _write( WARN, poster, msg );
    }

    public static void trace(String poster, String msg)
    {
        if( _enabled && _typeEnabled[TRACE] )
            _write( TRACE, poster, msg );
    }

    public static void log(String poster, String msg)
    {
        if( _enabled && _typeEnabled[LOG] )
            _write( LOG, poster, msg );
    }

    public static synchronized void _write(int type, String poster, String msg)
    {
        String buf = "";
        if( _showDate )
            buf += "[" + new Date().toString() + "] ";
        buf += "\"" + _typeName[type] + "\": ";
        if( null != poster )
            buf += "poster: " + "\"" + poster + "\" ";
        if( _showThreadName )
            buf += "thread: " + "\"" + Thread.currentThread().getName() + "\" ";
        buf += "msg: " + "\"" + msg + "\"";

        if( _logToFile )
            _writeToLogfile(buf+"\n");
        if( _logToConsole )
            System.out.println(buf);
    }

    public static boolean getEnabled()
    {
        return _enabled;
    }

    public static synchronized void setEnabled(boolean enabled)
    {
        _enabled = enabled;
    }

    public static boolean getEnabledType(int type)
    {
        return _typeEnabled[type];
    }

    public static synchronized void setEnabledType(int type, boolean enabled)
    {
        _typeEnabled[type] = enabled;
    }

    public static boolean getShowDate()
    {
        return _showDate;
    }

    public static synchronized void setShowDate(boolean b)
    {
        _showDate = b;
    }

    public static boolean getShowThreadName()
    {
        return _showThreadName;
    }

    public static synchronized void setShowThreadName(boolean b)
    {
        _showThreadName = b;
    }

    public static String getFilename()
    {
        return _fullFilename;
    }
    public static synchronized void setFilename(String filename, boolean useCodebase)
    {
        if(AS.S)ER.T(null==_fullFilename,"tried to set filename after first log entry written");
        _baseFilename = filename;
        _useCodebase = useCodebase;
        _generateFilename();
    }

    public static boolean getLogToFile()
    {
        return _logToFile;
    }
    public static synchronized void    setLogToFile(boolean logToFile)
    {
        _logToFile = logToFile;
    }

    public static boolean getLogToConsole()
    {
        return _logToConsole;
    }
    public static synchronized void    setLogToConsole(boolean logToConsole)
    {
        _logToConsole = logToConsole;
    }

    private static synchronized void _writeToLogfile(String s)
    {
        try
        {
            PrivilegeManager.enablePrivilege("UniversalFileWrite");
            PrivilegeManager.enablePrivilege("UniversalFileRead");
            _start();
            if( null != _out )
                _out.writeBytes(s);
            _finish();
        }
        catch( ForbiddenTargetException e )
        {
            // eat it;
        }
        catch( IOException e )
        {
            // eat it;
        }
    }

    private static boolean _start()
    {
        if( ! _generateFilename() )
            return false;
        if( null == _out )
        {
            try
            {
                _out = new DataOutputStream(new FileOutputStream(_fullFilename,true));
            }
            catch( IOException e )
            {
                // eat it
                _out = null;
            }
        }

        return null != _out;
    }

    private static void _finish()
    {
        if( null != _out )
        {
            try
            {
                _out.flush();
            }
            catch( IOException e )
            {
                // eat it
            }
        }
    }

    private static boolean _generateFilename()
    {
        if( null != _fullFilename )
            return true;

        if( _useCodebase )
        {
            String dir = Env.getPrefsDir();

            if( null == dir )
                return false;
            _fullFilename = dir + _baseFilename;
        }
        else
            _fullFilename = _baseFilename;
        return true;
    }


    private static boolean          _typeEnabled[]  = {false,false,false,true};
    private static final String     _typeName[]     = {"error","warning","trace","log"};

    private static boolean          _enabled        = true;
    private static boolean          _showDate       = true;
    private static boolean          _showThreadName = false;
    private static boolean          _logToFile      = true;
    private static boolean          _logToConsole   = false;
    private static DataOutputStream _out            = null;
    private static boolean          _useCodebase    = false;
    private static String           _fullFilename   = null;
    private static String           _baseFilename   = "jsdlog.log";
}

