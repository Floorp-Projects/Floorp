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

package com.netscape.jsdebugging.api.local;

public class ValueLocal
    implements com.netscape.jsdebugging.api.Value
{
    ValueLocal(netscape.jsdebug.Value val) {_val = val;}

    public boolean isObject()       {return _val.isObject();}
    public boolean isNumber()       {return _val.isNumber();}
    public boolean isInt()          {return _val.isInt();}
    public boolean isDouble()       {return _val.isDouble();}
    public boolean isString()       {return _val.isString();}
    public boolean isBoolean()      {return _val.isBoolean();}
    public boolean isNull()         {return _val.isNull();}
    public boolean isVoid()         {return _val.isVoid();}
    public boolean isPrimitive()    {return _val.isPrimitive();}
    public boolean isFunction()     {return _val.isFunction();}
    public boolean isNative()       {return _val.isNative();}

    public boolean getBoolean()     {return _val.getBoolean();}
    public int     getInt()         {return _val.getInt();}
    public double  getDouble()      {return _val.getDouble();}

    public String getString()       {return _val.getString();}
    public String getFunctionName() {return _val.getFunctionName();}
    public String getClassName()    {return _val.getClassName();}

    public com.netscape.jsdebugging.api.Value getPrototype()
    {
        if(null == _proto && !_getFlag(GOT_PROTO))
        {
            netscape.jsdebug.Value p = _val.getPrototype();
            if(null != p)
                _proto = new ValueLocal(p);
            _setFlag(GOT_PROTO);
        }
        return _proto;
    }

    public com.netscape.jsdebugging.api.Value getParent()
    {
        if(null == _parent && !_getFlag(GOT_PARENT))
        {
            netscape.jsdebug.Value p = _val.getParent();
            if(null != p)
                _parent = new ValueLocal(p);
            _setFlag(GOT_PARENT);
        }
        return _parent;
    }

    public com.netscape.jsdebugging.api.Value getConstructor()
    {
        if(null == _ctor && !_getFlag(GOT_CTOR))
        {
            netscape.jsdebug.Value c = _val.getConstructor();
            if(null != c)
                _ctor = new ValueLocal(c);
            _setFlag(GOT_CTOR);
        }
        return _ctor;
    }

    public com.netscape.jsdebugging.api.Property[] getProperties()
    {
        if(null == _props && !_getFlag(GOT_PROPS))
        {
            netscape.jsdebug.Property[] p = _val.getProperties();
            if(null != p && 0 != p.length)
            {
                _props = new PropertyLocal[p.length];
                for( int i = 0; i < p.length; i++)
                    _props[i] = new PropertyLocal(p[i]);
            }
            _setFlag(GOT_PROPS);
        }
        return _props;
    }

    public com.netscape.jsdebugging.api.Property getProperty(String name)
    {
        if(null == name)
            return null;
        com.netscape.jsdebugging.api.Property[] props = getProperties();
        if(null == props)
            return null;
        for(int i  = 0; i < props.length; i++)
            if(name.equals(props[i].getNameString()))
                return props[i];
        netscape.jsdebug.Property wprop = _val.getProperty(name);
        if(null == wprop)
            return null;
        com.netscape.jsdebugging.api.Property prop = new PropertyLocal(wprop);
        if(null != prop) {
            com.netscape.jsdebugging.api.Property[] newProps = 
                new com.netscape.jsdebugging.api.Property[props.length+1];
            System.arraycopy(props, 0, newProps, 0, props.length);
            newProps[props.length] = prop;
            _props = newProps;
        }
        return prop;
    }

    public String[] getPropertyHints() {return _val.getPropertyHints();}
    public void setPropertyHints(String[] hints) {_val.setPropertyHints(hints);}

    public void refresh() {
        _val.refresh();
        _flags  = 0;
        _props  = null;
        _proto  = null;
        _parent = null;
        _ctor   = null;
    }

    public netscape.jsdebug.Value getWrappedValue() {return _val;}

    // override Object
    public String toString()       {return _val.toString();}
    public int    hashCode()       {return _val.hashCode();}

    // private flagage...

    private static final short GOT_PROTO    = (short) (1 << 0);
    private static final short GOT_PROPS    = (short) (1 << 1);
    private static final short GOT_PARENT   = (short) (1 << 2);
    private static final short GOT_CTOR     = (short) (1 << 3);

    private final boolean _getFlag(short b)   {return 0 != (_flags & b);}
    private final void    _setFlag(short b)   {_flags |= b;}
    private final void    _clearFlag(short b) {_flags &= ~b;}
    private short _flags;

    // private data...
    
    private netscape.jsdebug.Value _val;

    private com.netscape.jsdebugging.api.Property[] _props;
    private com.netscape.jsdebugging.api.Value      _proto;
    private com.netscape.jsdebugging.api.Value      _parent;
    private com.netscape.jsdebugging.api.Value      _ctor;
}
