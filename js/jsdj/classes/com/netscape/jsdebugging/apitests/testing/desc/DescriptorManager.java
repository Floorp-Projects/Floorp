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

import java.util.*;
import com.netscape.jsdebugging.apitests.xml.*;

/** 
 * Manager a pool of descriptors.
 *
 * @author Alex Rakhlin
 */
public class DescriptorManager {

    public DescriptorManager (){
        _descriptors = new Hashtable();
    }

    /**
     * Add a new descriptor with Class c
     */
    public void add (Class c, Descriptor d){
        _descriptors.put (c, d);
    }

    /**
     * Get a descriptor corresponding to the class
     */
    public Descriptor getDescriptor (Class c){
        Object o =  _descriptors.get (c);
        if ( o == null ) {
            System.err.println ("Descriptor not found in the manager");
            return null;
        }
        return (Descriptor) o;
    }
    
    /**
     * Add standard descriptors. Need to include each descriptor which will be used.
     */
    public void addDescriptors (XMLWriter _xmlw){
        add (com.netscape.jsdebugging.apitests.testing.desc.DescPC.class, new DescPC (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescStep.class, new DescStep (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescLine.class, new DescLine (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescEval.class, new DescEval (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescLines.class, new DescLines (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescFrame.class, new DescFrame (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescError.class, new DescError (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescStack.class, new DescStack (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescScript.class, new DescScript (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescSection.class, new DescSection (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescSections.class, new DescSections (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescInterrupt.class, new DescInterrupt (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescScriptLoaded.class, new DescScriptLoaded (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescScriptUnLoaded.class, new DescScriptUnLoaded (_xmlw, this));
        add (com.netscape.jsdebugging.apitests.testing.desc.DescSourceLocation.class, new DescSourceLocation (_xmlw, this));
    }

    private Hashtable _descriptors;
}