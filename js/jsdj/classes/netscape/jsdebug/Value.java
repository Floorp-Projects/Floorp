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
* The Value class represents a JavaScript jsval which may be one of many types
* and may or may not have properties
*
* @author  John Bandhauer
* @version 1.1
* @since   1.1
*/

public final class Value {

    private Value() {}  // can only be constructed natively

    public native boolean isObject();
    public native boolean isNumber();
    public native boolean isInt();
    public native boolean isDouble();
    public native boolean isString();
    public native boolean isBoolean();
    public native boolean isNull();
    public native boolean isVoid();
    public native boolean isPrimitive();
    public native boolean isFunction();
    public native boolean isNative();

    public native boolean getBoolean();
    public native int     getInt();
    public native double  getDouble();

    /**                                                                  
    * does a JavaScript toString conversion if value is not already a string     
    */                                                                   
    public String getString() {
        if(null == _string)
            _string = _getString();
        return _string;
    }

    public String getFunctionName() {
        if(null == _functionName && isFunction())
            _functionName = _getFunctionName();
        return _functionName;
    }

    public Value getPrototype() {
        if(null == _proto && !_getFlag(GOT_PROTO) && isObject()) {
            _proto = _getPrototype();
            _setFlag(GOT_PROTO);
        }
        return _proto;
    }

    public Value getParent() {
        if(null == _parent && !_getFlag(GOT_PARENT) && isObject()) {
            _parent = _getParent();
            _setFlag(GOT_PARENT);
        }
        return _parent;
    }

    public Value getConstructor() {
        if(null == _ctor && !_getFlag(GOT_CTOR) && isObject()) {
            _ctor = _getConstructor();
            _setFlag(GOT_CTOR);
        }
        return _ctor;
    }

    public String getClassName() {
        if(null == _className && isObject()) {
            _className = _getClassName();
        }
        return _className;
    }

    public Property[] getProperties() {
        if(null == _props && !_getFlag(GOT_PROPS) && isObject()) {
            _props = _getProperties();
            _setFlag(GOT_PROPS);

            // if we have any hints, then try to find or add their props
            if(null != _hints && 0 != _hints.length) {
                Property[] newProps = new Property[_hints.length];
                int made = 0;
       do_prop: for(int k = 0; k < _hints.length; k++) {
                    String name = _hints[k];
                    if(null == name || 0 == name.length())
                        continue;
                    // see if it already exists
                    if(null != _props) {
                        for(int i = 0; i < _props.length; i++)
                            if(name.equals(_props[i].getNameString()))
                                continue do_prop;
                    }
                    // not in our array, try to get the named prop
                    Property prop = _getProperty(name);
                    if(null != prop) 
                        newProps[made++] = prop;
                }
                if(0 != made) {
                    if(null != _props) {
                        Property[] combinedProps = 
                                        new Property[_props.length+made];
                        System.arraycopy(_props, 0, combinedProps, 0, _props.length);
                        System.arraycopy(newProps, 0, combinedProps, _props.length, made);
                        _props = combinedProps;
                    }
                    else {
                        Property[] realProps = new Property[made];
                        System.arraycopy(newProps, 0, realProps, 0, made);
                        _props = realProps;
                    }
                }
            }
        }
        return _props;
    }

    public Property getProperty(String name) {
        if(null == name)
            return null;
        Property[] props = getProperties();
        if(null == props)
            return null;
        for(int i  = 0; i < props.length; i++)
            if(name.equals(props[i].getNameString()))
                return props[i];
        Property prop = _getProperty(name);
        if(null != prop) {
            Property[] newProps = new Property[props.length+1];
            System.arraycopy(props, 0, newProps, 0, props.length);
            newProps[props.length] = prop;
            _props = newProps;
        }
        return prop;
    }

    public String[] getPropertyHints() {return _hints;}
    public void     setPropertyHints(String[] hints) {_hints = hints;}

    public void refresh() {
        _refresh();
        _flags           = 0;
        _string          = null;
        _functionName    = null;
        _className       = null;
        _props           = null;
        _proto           = null;
        _parent          = null;
        _ctor            = null;
        _string4toString = null;
    }

    // override Object
    public boolean equals(Object obj) {
        try {
            return _equals(_nativeJSVal, ((Value)obj)._nativeJSVal);
        }
        catch(ClassCastException e) {
            return false;
        }
    }
    public int hashCode() {return _nativeJSVal;}

    public String toString() {
        if(null == _string4toString) {
            _string4toString = _typeString();
            _string4toString += " ";
            _string4toString += _shortValString(this);
            if(isObject() && !isNull()) {
                _string4toString += ", ";
                _string4toString += _countOfProps() + " props";
                if(isFunction()) {
                    _string4toString += ", ";
                    _string4toString += "functionName = " + getFunctionName();
                }
                _string4toString += ", ";
                _string4toString += "classname = " + getClassName();
                _string4toString += ", ";
                _string4toString += "proto = " + _shortValString(getPrototype());
                _string4toString += ", ";
                _string4toString += "parent = " + _shortValString(getParent());
                _string4toString += ", ";
                _string4toString += "ctor = " + _shortValString(getConstructor());
            }
        }
        return _string4toString;
    }

    // private toString formatting stuff...

    private String _shortValString(Value v) {
        if(null == v)
            return "null";
        else if(v.isFunction())
            return "[function "+v.getFunctionName()+"]";
        else
            return v.getString();
    }

    private int _countOfProps() {
        Property[] p = getProperties();
        if(null == p)
            return 0;
        return p.length;
    }

    private String _typeString() {
        StringBuffer sb = new StringBuffer(11);
        sb.append(isObject()    ? 'o' : '.');
        sb.append(isNumber()    ? 'n' : '.');
        sb.append(isInt()       ? 'i' : '.');
        sb.append(isDouble()    ? 'd' : '.');
        sb.append(isString()    ? 's' : '.');
        sb.append(isBoolean()   ? 'b' : '.');
        sb.append(isNull()      ? 'N' : '.');
        sb.append(isVoid()      ? 'V' : '.');
        sb.append(isPrimitive() ? 'p' : '.');
        sb.append(isFunction()  ? 'f' : '.');
        sb.append(isNative()    ? 'n' : '.');
        return sb.toString();
    }

    // private flagage...

    private static final short GOT_PROTO    = (short) (1 << 0);
    private static final short GOT_PROPS    = (short) (1 << 1);
    private static final short GOT_PARENT   = (short) (1 << 2);
    private static final short GOT_CTOR     = (short) (1 << 3);

    private final boolean _getFlag(short b)   {return 0 != (_flags & b);}
    private final void    _setFlag(short b)   {_flags |= b;}
    private final void    _clearFlag(short b) {_flags &= ~b;}
    private short _flags;

    // private natives...

    protected native void finalize();
    private native void _refresh();
    private native String _getString();
    private native String _getFunctionName();
    private native String _getClassName();
    private native Property[] _getProperties();
    private native Value _getPrototype();
    private native Value _getParent();
    private native Value _getConstructor();
    private native boolean _equals(int nativeJSVal, int other_nativeJSVal);
    private native Property _getProperty(String name);

    // private data...

    private int        _nativeJSVal;
    private String     _string;
    private String     _functionName;
    private String     _className;
    private Property[] _props;
    private Value      _proto;
    private Value      _parent;
    private Value      _ctor;
    private String     _string4toString;
    private String[]   _hints;
}
