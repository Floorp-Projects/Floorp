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