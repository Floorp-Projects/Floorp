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
* The Property class represents a single property of a JavaScript Value.
*
* @author  John Bandhauer
* @version 1.1
* @since   1.1
*/

public final class Property {

    private Property() {}   // can only be made natively

    // these must stay the same as the flags in jsdbgapi.h

    public static int ENUMERATE  = 0x01;    /* visible to for/in loop */
    public static int READONLY   = 0x02;    /* assignment is error */
    public static int PERMANENT  = 0x04;    /* property cannot be deleted */
    public static int ALIAS      = 0x08;    /* property has an alias id */
    public static int ARGUMENT   = 0x10;    /* argument to function */
    public static int VARIABLE   = 0x20;    /* local variable in function */
    public static int HINTED     = 0x800;   /* found via explicit lookup */


    public boolean isNameString() {return 0 != (_internalFlags & NAME_IS_STRING);}
    public boolean isAliasString(){return 0 != (_internalFlags & ALIAS_IS_STRING);}

    public String getNameString() {
        if(null == _nameString && !isNameString())
            _nameString = Integer.toString(_nameInt);
        return _nameString;
    }
    public int getNameInt() {return _nameInt;}

    public String getAliasString() {
        if(!hasAlias())
            return null;
        if(null == _aliasString && !isAliasString())
            _aliasString = Integer.toString(_aliasInt);
        return _aliasString;
    }    
    public int getAliasInt() {return _aliasInt;}

    public Value getValue() {return _value;}

    public int getVarArgSlot() {return _slot;}

    public int     getFlags()     {return _flags;}
    public boolean canEnumerate() {return 0 != (_flags & ENUMERATE);}
    public boolean isReadOnly()   {return 0 != (_flags & READONLY );}
    public boolean isPermanent()  {return 0 != (_flags & PERMANENT);}
    public boolean hasAlias()     {return 0 != (_flags & ALIAS    );}
    public boolean isArgument()   {return 0 != (_flags & ARGUMENT );}
    public boolean isVariable()   {return 0 != (_flags & VARIABLE );}
    public boolean isHinted()     {return 0 != (_flags & HINTED   );}


    // override Object
    public String toString() {
        if(null == _string4toString) {
            _string4toString = _flagsString();
            _string4toString += " name = "+ getNameString();
            if(hasAlias())
                _string4toString += ", alias = " + getAliasString();
            if(isArgument() || isVariable())
                _string4toString += ", vararg slot = " + getVarArgSlot();
            _string4toString += ", val = " + _shortValString(_value);
        }
        return _string4toString;
    }

    private String _shortValString(Value v) {
        if(null == v)
            return "null";
        else if(v.isFunction())
            return "[function "+v.getFunctionName()+"]";
        else
            return v.getString();
    }

    private String _flagsString() {
        StringBuffer sb = new StringBuffer(7);
        sb.append(canEnumerate() ? 'e' : '.');
        sb.append(isReadOnly()   ? 'r' : '.');
        sb.append(isPermanent()  ? 'p' : '.');
        sb.append(hasAlias()     ? 'a' : '.');
        sb.append(isArgument()   ? 'A' : '.');
        sb.append(isVariable()   ? 'V' : '.');
        sb.append(isHinted()     ? 'H' : '.');
        return sb.toString();
    }

    // private data...

    private Value   _value;
    private String  _nameString;
    private int     _nameInt;

    private String  _aliasString;
    private int     _aliasInt;

    private int     _flags;
    private int     _slot;

    private static final short NAME_IS_STRING   = 0x01;
    private static final short ALIAS_IS_STRING  = 0x02;
    private short   _internalFlags;

    private String  _string4toString;
}