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