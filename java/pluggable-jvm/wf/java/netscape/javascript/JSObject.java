/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: JSObject.java,v 1.1 2001/07/12 20:25:45 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package netscape.javascript;

import java.applet.Applet;
import java.applet.AppletContext;


import sun.jvmp.javascript.JSContext; 

public abstract class JSObject {

    /**
     * Call to JS 
     **/
    public abstract Object call(String methodName, 
				Object args[]) 
	throws JSException;

    /** 
     * Evaluate JS expression
     */
    public abstract Object eval(String s) throws JSException;

    /**
     * Obtain member of JS object
     */
    public abstract Object getMember(String name) throws JSException;

    /** 
     * Set member of JS object
     */
    public abstract void setMember(String name, Object value) throws JSException;

    /**
     * Remove member of JS object
     */
    public abstract void removeMember(String name) throws JSException;

    /**
     * Obtain slot of JS object
     */
    public abstract Object getSlot(int index) throws JSException;

    /**
     * Set slot of JS object
     */
    public abstract void setSlot(int index, Object value) throws JSException;

    /** 
     * Returns a JSObject for the window containing the given applet. 
     */
    public static JSObject getWindow(Applet applet) throws JSException {

	AppletContext c = applet.getAppletContext();

        JSObject ret = null;

        if (c instanceof sun.jvmp.javascript.JSContext)
	{
	    JSContext j = (JSContext) c;
	    ret = j.getJSObject();
	}

        if (ret == null)
           throw new JSException("AppletContext doesn\'t provide correct JSContext");
        else
           return ret;
    }
}
