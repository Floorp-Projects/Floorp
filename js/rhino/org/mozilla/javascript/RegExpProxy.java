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

/**
 * A proxy for the regexp package, so that the regexp package can be
 * loaded optionally.
 * 
 * @author Norris Boyd
 */
public interface RegExpProxy {
    
    public Object executeRegExp(Object regExp, Scriptable scopeObj, 
                                String str, int indexp[], boolean test);
    
    public boolean isRegExp(Object obj);
    
    public Object match(Context cx, Scriptable thisObj, Object[] args, 
                        Function funObj)
        throws JavaScriptException;

    public Object search(Context cx, Scriptable thisObj,  Object[] args,
                         Function funObj)
        throws JavaScriptException;

    public Object replace(Context cx, Scriptable thisObj, Object[] args, 
                          Function funObj)
        throws JavaScriptException;
 
    public int find_split(Function funObj, String target, String separator, 
                          Object re, int[] ip, int[] matchlen, 
                          boolean[] matched, String[][] parensp);
}
