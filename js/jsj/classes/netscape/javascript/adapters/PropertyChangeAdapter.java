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

/*
 * PropertyChangeAdapter.java
 *
 */

package netscape.javascript.adapters;

import netscape.javascript.JSObject;

import java.beans.PropertyChangeListener;
import java.beans.PropertyChangeEvent;

/**
 * PropertyChangeAdaptor allows PropertyChangeEvents to be handled in JavaScript.
 * It implements java.beans.PropertyChangeListener which is required in order to
 * order to catch PropertyChangeEvents, but impossible to do in JavaScript due to
 * Java's type safe implementation.  Once the event is caught, it is passed along
 * to a JavaScript method on a JavaScript object (JSObject). This method is
 * "onChange" by default, or an arbitrary method name can be supplied.
 */
 public class PropertyChangeAdapter implements PropertyChangeListener {

    /**
     *  Construct a PropertyChangeAdaptor for given JavaScript object.
     *  propertyChange events will be relayed to a method called onChange on
     *  the JavaScript object.  This onChange method must have a single parameter,
     *  which is a java.beans.PropertyChangeEvent object.
     */
    public PropertyChangeAdapter( JSObject oJS ) {
        moJSObject = oJS;
    }

    /**
     *  Construct a PropertyChangeAdaptor for given JavaScript object.
     *  propertyChange events will be relayed to a the specified method on
     *  the JavaScript object.  The method must have a single parameter, which
     *  is a java.beans.PropertyChangeEvent object.
     */
    public PropertyChangeAdapter( JSObject oJS, String sJSMethodName ) {
        moJSObject = oJS;
        msJSMethodName = sJSMethodName;
    }

    /**
     * propertyChange() is called by a source component (bean) when any of
     * its "bound" properties are altered.  We catch it and call a JavaScript
     * method so that handling code can occur using user's JavaScript.
     */
    public void propertyChange( PropertyChangeEvent oEvent ) {
        if ( null != moJSObject ) {
            // don't try to call mehtod which does not exist!
            Object oMethod = moJSObject.getMember( msJSMethodName );
            if ( null != oMethod ) {
                // pass oEvent as single parameter
                Object[] aArgs = new Object[1];
                aArgs[0] = oEvent;
                try {
                    moJSObject.call( msJSMethodName, aArgs );
                } catch (Exception ex) {
                    System.out.println("Exception: " + ex);
                    ex.printStackTrace();
                }
            }
        }
    }

    //-------------------------------------------------------------
    // member variables

    // ---------- static variables --------------------------------
    public static final String gsDefaultJSMethodName = "onChange";

    // ---------- instance variables ------------------------------
    private String      msJSMethodName   = gsDefaultJSMethodName;
    private JSObject    moJSObject       = null;
}
