/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

import org.mozilla.javascript.*;

public class Counter extends ScriptableObject {
    // The zero-argument constructor used by Rhino runtime to create instances
    public Counter() { }

    // Method jsConstructor defines the JavaScript constructor 
    public void jsConstructor(int a) { count = a; }

    // The class name is defined by the getClassName method
    public String getClassName() { return "Counter"; }

    // The method jsGet_count defines the count property. 
    public void jsFunction_resetCount() { count = 0; }

    // Methods can be defined using the jsFunction_ prefix. Here we define 
    //  resetCount for JavaScript. 
    public int jsGet_count() { return count++; }

    private int count;
}
