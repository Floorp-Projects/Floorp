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
  
package com.netscape.jsdebugging.apitests.testing.desc;

import com.netscape.jsdebugging.apitests.xml.*;
import com.netscape.jsdebugging.apitests.*;
import java.util.*;

/** 
 * Parent of all serializable descriptors.
 * Keeps a hashtable of objects that were described, indexed by serial numbers.
 *
 * @author Alex Rakhlin
 */
public abstract class DescriptorSerializable extends Descriptor {

    public DescriptorSerializable (String tag, XMLWriter xmlw, DescriptorManager dmgr){
        super (tag, xmlw, dmgr);
        _serial = new Hashtable ();
        _ser_num = 0;
    }
    
    public void describe (Object obj){
        _xmlw.startTag (getTag());
        int i = getSerialNumber (obj);
        if (i == -1) {
            _xmlw.startTag (Tags.description_tag);
            _xmlw.tag (Tags.type_tag, getTag());
            if (obj != null) _xmlw.tag (Tags.serial_number_tag, _ser_num);
            else _xmlw.tag (Tags.serial_number_tag, -1);
            description (obj);
            _xmlw.endTag (Tags.description_tag);
            if (obj != null) {
                _xmlw.tag (Tags.serial_number_tag, _ser_num);
                insert (obj);
            } else _xmlw.tag (Tags.serial_number_tag, -1);
        }
        else {
            _xmlw.tag (Tags.serial_number_tag, i);
            /*
            _xmlw.startTag (Tags.check_tag);
            description (obj);
            _xmlw.endTag (Tags.check_tag);
            */
        }
        _xmlw.endTag (getTag());
    }
    
    /**
     * Given an object, get it's serial number. Returns -1 if object is not present
     */
    public int getSerialNumber (Object o) {
        if (o == null) return -1;
        Integer i = (Integer)_serial.get (o);
        if (i == null) return -1;
        return i.intValue();
    }

    /**
     * Inserts a new object into the hashtable and increase the current serial number
     */
    public void insert (Object o) {
        _serial.put (o, new Integer (_ser_num));
        _ser_num ++;
    }

    public void remove (Object o){  _serial.remove (o);  }

    public Hashtable getSerialNumbers () { return _serial; }
    
    private Hashtable _serial;
    private int _ser_num;
}