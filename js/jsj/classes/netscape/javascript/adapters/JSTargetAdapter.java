/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* ** */

package netscape.javascript.adapters;

import netscape.javascript.*;
import netscape.application.Target;

/**
 * JSTargetAdapter is used in JavaScript code to deliver target
 * commands to a JavaScript object.
 */
public final class JSTargetAdapter implements Target {
    /* the real target object in JavaScript */
    private JSObject jsObj;

    /**
     * construct a new JSTargetAdapter for a given JSObject
     */
    public JSTargetAdapter(JSObject jsObj) {
        this.jsObj = jsObj;
    }

    /**
     * Check to see whether the JavaScript object has an ["on" + command]
     * member, and if so invoke it with the data parameter.
     */
    public void performCommand(String command, Object data) {
        if (jsObj == null)
            return;

        Object member = jsObj.getMember("on" + command);
        if (member == null)
            return;

        Object[] args;
        args = new Object[1];
        args[0] = data;

        try {
            jsObj.call("on" + command, args);
        } catch (Exception e) {
            System.out.println("Exception: " + e);
            e.printStackTrace();
        }
    }
}
