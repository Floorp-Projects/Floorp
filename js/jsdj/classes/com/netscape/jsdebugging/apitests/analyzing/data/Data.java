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

package com.netscape.jsdebugging.apitests.analyzing.data;

/**
 * This is a parent of all data classes which contain data from the log file describing an object.
 *
 * @author Alex Rakhlin
 */
 
public abstract class Data {
    
    public Data (DataPoolManager dpm){
        _ignore = false;
        _dpm = dpm;
    }    
    
    public String toString (){
        return "override this method";
    }

    public String toFormattedString (){
        return "override this method";
    }
    
    /* must override this method */
    public abstract boolean equalsTo (Data d);
    
    /**
     * should this object be ignored?
     */
    public boolean getIgnore () { return _ignore; }
    
    /**
     * Set the ignore field of this object to true.
     */
    public void ignore () { _ignore = true; }
        
    /* ignore flag */
    protected boolean _ignore;
    
    /* reference to the pool manager -- might need it */
    protected DataPoolManager _dpm;
    
}