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