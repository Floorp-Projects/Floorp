/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package com.netscape.jsdebugging.api.local;

public class PropertyLocal
    implements com.netscape.jsdebugging.api.Property
{
    PropertyLocal(netscape.jsdebug.Property prop) {_prop = prop;}

    public boolean isNameString()   {return _prop.isNameString();}
    public boolean isAliasString()  {return _prop.isAliasString();}

    public String getNameString()   {return _prop.getNameString();}
    public int getNameInt()         {return _prop.getNameInt();}

    public String getAliasString()  {return _prop.getAliasString();}
    public int getAliasInt()        {return _prop.getAliasInt();}

    public com.netscape.jsdebugging.api.Value getValue()
    {
        if(null == _val)
            _val = new ValueLocal(_prop.getValue());
        return _val;
    }

    public int getVarArgSlot()      {return _prop.getVarArgSlot();}

    public int     getFlags()       {return _prop.getFlags();}
    public boolean canEnumerate()   {return _prop.canEnumerate();}
    public boolean isReadOnly()     {return _prop.isReadOnly();}
    public boolean isPermanent()    {return _prop.isPermanent();}
    public boolean hasAlias()       {return _prop.hasAlias();}
    public boolean isArgument()     {return _prop.isArgument();}
    public boolean isVariable()     {return _prop.isVariable();}
    public boolean isHinted()       {return _prop.isHinted();}

    // override Object
    public String toString()        {return _prop.toString();}

    private netscape.jsdebug.Property _prop;
    private com.netscape.jsdebugging.api.Value _val;
}