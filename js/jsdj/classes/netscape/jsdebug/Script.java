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

package netscape.jsdebug;

/**
* This instances of this class represent the JavaScript string object. This
* class is intended to only be constructed by the underlying native code.
* The DebugController will construct an instance of this class when scripts
* are created and that instance will always be used to reference the underlying
* script throughout the lifetime of that script.
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public final class Script
{
    public String   getURL()            {return _url;           }
    public String   getFunction()       {return _function;      }
    public int      getBaseLineNumber() {return _baseLineNumber;}
    public int      getLineExtent()     {return _lineExtent;    }
    public boolean  isValid()           {return 0 != _nativePtr;}

    /**
    * Get the PC of the first executable code on or after the given 
    * line in this script. returns null if none
    */
    public native JSPC      getClosestPC(int line);

    public String toString()
    {
        int end = _baseLineNumber+_lineExtent-1;
        if( null == _function )
            return "<Script "+_url+"["+_baseLineNumber+","+end+"]>";
        else
            return "<Script "+_url+"#"+_function+"()["+_baseLineNumber+","+end+"]>";
    }

    public int hashCode()
    {
        return _nativePtr;
    }

    public boolean equals(Object obj)
    {
        if( obj == this )
            return true;

        // In our system the native code is the only code that generates
        // these objects. They are never duplicated
        return false;
/*
        if( !(obj instanceof Script) )
            return false;
        return 0 != _nativePtr && _nativePtr == ((Script)obj)._nativePtr;
*/
    }

    private synchronized void _setInvalid() {_nativePtr = 0;}

    private String  _url;
    private String  _function;
    private int     _baseLineNumber;
    private int     _lineExtent;
    private int     _nativePtr;     // used internally
}    
