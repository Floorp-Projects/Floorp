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
  
package com.netscape.jsdebugging.apitests.testing.desc;

import com.netscape.jsdebugging.apitests.xml.*;

/**
 * Parent of all descriptors.
 *
 * @author Alex Rakhlin
 */ 
public abstract class Descriptor {

    public Descriptor (String tag, XMLWriter xmlw, DescriptorManager dmgr){
        _tag = tag;
        _xmlw = xmlw;
        _dmgr = dmgr;
    }
    
    public String toString () { return _tag; }
    public String getTag () { return _tag; }
    public DescriptorManager getDmgr () { return _dmgr; }
    
    public void describe (Object obj){
        _xmlw.startTag (getTag());
        description (obj);
        _xmlw.endTag (getTag());
    }
    
    public void describe (Object obj, int i){
        // another version for DescLine object -- takes Script and lineno
        _xmlw.startTag (getTag());
        description (obj, i);
        _xmlw.endTag (getTag());
    }
    
    
    /** 
     * Should override this function to output the description. This function will be called 
     * by the describe() method.
     */
    public void description (Object obj){
    }
    
    /**
     * Same as above, but for two objects -- object and integer. In fact, can pack this into
     * one object.
     */
    public void description (Object obj, int i){
    }
        
    protected XMLWriter _xmlw;
    private String _tag;
    private DescriptorManager _dmgr;
}