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
* 'Model' to manage inspector info about a singe value
*/

// when     who     what
// 10/30/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;

public class InspectorNodeModel
{
    public InspectorNodeModel( String name,
                               int depth,
                               InspectorNodeModel parent,
                               InspectorTyrant tyrant )
    {
        _name = name.trim();
        _depth = depth;
        _parent = parent;
        _tyrant = tyrant;
        String fullname = getFullName();
        _type = _tyrant.eval("typeof "+fullname);

        if( "function".equals(_type) )
        {
            _value = "[function]";
            _hasProperties = false;
        }
        else
        {
            _value = Util.expandTabs( _tyrant.eval(fullname), 8 );
//            _hasProperties = "object".equals(_type)
//                        && ! "[null]".equals(_value);
            if(! _value.equals("[null]"))
                if(_type.startsWith("object") || _type.startsWith("array"))
                    _hasProperties = true;
        }
    }

    public synchronized int expand()
    {
        if( null != _firstChild )
        {
            _firstChild.clearLinks();
            _firstChild = null;
        }

        InspectorNodeModel prev = null;
        int childCount = 0;
        String[] childNames = _tyrant.getPropNamesOfJavaScriptThing(getFullName());
        if( null != childNames && 0 != (childCount = childNames.length) )
        {
            for(int i = 0; i < childCount; i++ )
            {
                InspectorNodeModel cur = new InspectorNodeModel(childNames[i],
                                                                _depth + 1,
                                                                this,
                                                                _tyrant);
                if( null == _firstChild )
                    _firstChild = cur;
                if( null != prev )
                    prev._nextSib = cur;
                prev = cur;
            }
        }
        return childCount;
    }

    public synchronized void clearLinks()
    {
        _parent = null;
        if( null != _firstChild )
        {
            _firstChild.clearLinks();
            _firstChild = null;
        }
        if( null != _nextSib )
        {
            _nextSib.clearLinks();
            _nextSib = null;
        }
    }

    public String  getName()        {return _name;}
    public String  getValue()       {return _value;}
    public String  getType()        {return _type;}
    public int     getDepth()       {return _depth;}
    public boolean hasProperties()  {return _hasProperties;}
    public String  getFullName()
    {
        if( null == _fullname )
        {
            if( null != _parent )
            {
                _fullname = _parent.getFullName();
                if( ! _name.startsWith("[") )
                    _fullname += ".";
                _fullname += _name;
            }
            else
                _fullname = _name;
        }
        return _fullname;
    }
    public InspectorNodeModel  getParent()     {return _parent;}
    public InspectorNodeModel  getFirstChild() {return _firstChild;}
    public InspectorNodeModel  getNextSib()    {return _nextSib;}

    public boolean isAncestor(InspectorNodeModel model)
    {
        InspectorNodeModel p = _parent;
        while(null != p)
        {
            if( p == model )
                return true;
            p = p.getParent();
        }
        return false;
    }

    private InspectorTyrant     _tyrant;

    private InspectorNodeModel  _parent;
    private InspectorNodeModel  _firstChild;
    private InspectorNodeModel  _nextSib;
    private String              _fullname;
    private String              _name;
    private int                 _depth;
    private String              _value;
    private boolean             _hasProperties;
    private String              _type;
    private boolean             _isValid;
}
