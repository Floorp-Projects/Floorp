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
 * $Id: WFAppletViewer.java,v 1.1 2001/07/12 20:26:01 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.applet;

import java.net.URL;
import java.awt.Frame;
import java.applet.Applet;
/**
 * interface to be implemented by different applet viewer implementation
 **/
public interface  WFAppletViewer 
{    
    /**
     * applet lifetime control 
     **/
    public void   startApplet();
    public void   stopApplet();
    // timeout is time allowed for applet to complete its shutdown
    // after all it just got destroyed
    public void   destroyApplet(int timeout);

    /**
     * Returns current status of applet loading, possible values - see below
     **/
    public int    getLoadingStatus();

    /**
     * Get currently viewed object
     */
    public Applet getApplet();
    
    /**
     * two methods to be used to supply applet with
     * rendering context and real document base later than it created.
     * Applet viewer shouldn't try to start applet before both are supplied.
     **/
    public void   setDocumentBase(URL docbase);
    public void   setWindow(Frame f);
    
    public final static String CID = "@sun.com/wf/applets/appletviewer;1";
    public final static int    STATUS_STARTED = 1;    
    public final static int    STATUS_ERROR   = 2;
    public final static int    STATUS_UNKNOWN = 3;
    public final static int    STATUS_LOADING = 4;
    public final static int    STATUS_STOPPED = 5;    
};
