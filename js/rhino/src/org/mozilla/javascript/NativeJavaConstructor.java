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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;

import java.lang.reflect.*;

/**
 * This class reflects a single Java constructor into the JavaScript 
 * environment.  It satisfies a request for an overloaded constructor,
 * as introduced in LiveConnect 3.
 * All NativeJavaConstructors behave as JSRef `bound' methods, in that they
 * always construct the same NativeJavaClass regardless of any reparenting 
 * that may occur.
 *
 * @author Frank Mitchell
 * @see NativeJavaMethod
 * @see NativeJavaPackage
 * @see NativeJavaClass
 */

public class NativeJavaConstructor extends NativeFunction implements Function {

    public NativeJavaConstructor(Constructor ctor) {
        this.constructor = ctor;
        names = new String[1];
        names[0] = "<init>" + NativeJavaMethod.signature(ctor);
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        // Find a method that matches the types given.
        if (constructor == null) {
            throw new RuntimeException("No constructor defined for call");
        }

        // Eliminate useless args[0] and unwrap if required
        for (int i = 0; i < args.length; i++) {
            if (args[i] instanceof Wrapper) {
                args[i] = ((Wrapper)args[i]).unwrap();
            }
        }

        return NativeJavaClass.constructSpecific(cx, scope, 
                                                 this, constructor, args);
    }

    /*
    public Object getDefaultValue(Class hint) {
        return this;
    }
    */

    public String toString() {
        return "[JavaConstructor " + constructor.getName() + "]";
    }

    Constructor getConstructor() {
        return constructor; 
    }

    Constructor constructor;
}

