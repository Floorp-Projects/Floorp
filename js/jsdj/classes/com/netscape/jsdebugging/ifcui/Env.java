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
* Platform specific support for detecting platform and locating prefs files
*/

// when     who     what
// 12/06/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import java.io.File;

/* A place to get info about the application Environment */

// all statics...
public class Env
{
    public static final int ENV_WIN     = 1;
    public static final int ENV_MAC     = 2;
    public static final int ENV_UNIX    = 3;

    public static void Init()
    {
        _initPlatformType();
        _initCodebaseDir();
        _initPrefsDir();
    }

    public static int getCoursePlatformType()
    {
        if( 0 == _coursePlatformType )
            _initPlatformType();
        return _coursePlatformType;
    }

    public static String getPrefsDir()
    {
        if( null == _prefsDir )
            _initPrefsDir();
        return _prefsDir;
    }

    public static String getCodebaseDir()
    {
        if( null == _codebaseDir )
            _initCodebaseDir();
        return _codebaseDir;
    }

    private static void _initPlatformType()
    {
        if( 0 != _coursePlatformType )
            return;
        try
        {
            String os = System.getProperty("os.name").toUpperCase();
            // System.out.println("System.getProperty(\"os.name\").toUpperCase() returned: " + os);
            if( os.startsWith("WIN") )
                _coursePlatformType = ENV_WIN;
            else if( os.startsWith("MAC") )
                _coursePlatformType = ENV_MAC;
            else
                _coursePlatformType = ENV_UNIX;
        }
        catch(Exception e)
        {
            System.out.println(e);
            System.out.println("Failed in getting os.name, defaulting to Unix");
            _coursePlatformType = ENV_UNIX;
        }
    }

    private static void _initCodebaseDir()
    {
        if( null != _codebaseDir )
            return;

        Application app = Application.application();
        if( null == app )
        {
            System.out.println("Failed in getting app, codebasedir set to /");
            return;
        }
        // get codebase
        java.net.URL loadDirURL = app.codeBase();
        String loadDir = loadDirURL.getFile();
        if(null != loadDir)
            _codebaseDir = loadDir.replace('/', File.separatorChar);

        // unescape the path
        StringBuffer sb = new StringBuffer();
        int len = _codebaseDir.length();
        for(int i = 0; i < len; i++ )
        {
            char c = _codebaseDir.charAt(i);
            if('%' == c && i+2 < len)
            {
                int high = Character.digit(_codebaseDir.charAt(i+1),16);
                int low = Character.digit(_codebaseDir.charAt(i+2),16);

                if(-1 != high && -1 != low)
                {
                    c = (char) ((high * 16) + low);
                    i += 2;
                }
            }
            sb.append(c);
        }
        _codebaseDir = sb.toString();

        _initPlatformType();

        // hack to lose the leading slash in Windows filenames
        if( ENV_WIN == _coursePlatformType               &&
            _codebaseDir.length() >= 3                   &&
            _codebaseDir.charAt(0) == File.separatorChar &&
            (_codebaseDir.charAt(2) == ':' || _codebaseDir.charAt(2) == '|') )
        {
            _codebaseDir = _codebaseDir.substring(1);
        }

        // hack to lose the leading slash in Mac filenames
        //
        //  REMOVED! Mac Java in Nav wants leading slash!
        //
        // if( ENV_MAC == _coursePlatformType  &&
        //     _codebaseDir.length() >= 2      &&
        //     _codebaseDir.charAt(0) == File.separatorChar )
        // {
        //     _codebaseDir = _codebaseDir.substring(1);
        // }

        //
        // force leading slash on Mac
        //
        if( ENV_MAC == _coursePlatformType  &&
            _codebaseDir.length() >= 2      &&
            _codebaseDir.charAt(0) != File.separatorChar )
        {
            _codebaseDir = File.separatorChar + _codebaseDir;
        }

        // hack to get a 'normal' filename on Windows
        if( ENV_WIN == _coursePlatformType && _codebaseDir.charAt(1) == '|' )
            _codebaseDir = _codebaseDir.charAt(0)+":"+ _codebaseDir.substring(2);
    }

    private static void _initPrefsDir()
    {
        if( null != _prefsDir )
            return;

        _initPlatformType();
        _initCodebaseDir();

        // set default
        _prefsDir = _codebaseDir;
        if( null == _prefsDir )
            _prefsDir = "/";

        String s = Application.application().parameterNamed("PrefsDir");
        if( null != s )
        {
            _prefsDir = s;
            System.out.println("JSD set prefs dir from applet tag");
        }
        else if( ENV_MAC == _coursePlatformType && _codebaseDir.length() > 1 )
        {
            // is a Mac, try to use 'System Folder/Preferences'

            // use codebase for Volume name
            int sep = _codebaseDir.indexOf(File.separatorChar,1);
            if(-1 != sep)
            {
                _prefsDir = _codebaseDir.substring(0,sep+1) +
                            "System Folder"                 +
                            File.separatorChar              +
                            "Preferences"                   +
                            File.separatorChar;
            }
        }
    }


    private static int      _coursePlatformType = 0;
    private static String   _codebaseDir        = null;
    private static String   _prefsDir           = null;
}
