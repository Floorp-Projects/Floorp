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
 * $Id: WFAppletViewerImpl.java,v 1.1 2001/07/12 20:26:07 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.av;

import sun.jvmp.*;
import sun.jvmp.applet.*;
import java.util.Hashtable;
import java.applet.*;
import java.net.URL;
import java.awt.Frame;

class WFAppletViewerImpl implements WFAppletViewer
{
    WFAppletPanel av;
    
    
    WFAppletViewerImpl(PluggableJVM jvm, 
		       WFAppletContext ctx,
		       BrowserSupport support, 
		       Hashtable atts)
    {
	av = new WFAppletPanel(jvm, ctx, atts);
    }
    
    public void   startApplet()
    {
	av.appletStart();
    }
    
    public void   stopApplet()
    {
	av.appletStop();
    }
    
    public void   destroyApplet(int timeout)
    {
	av.onClose(timeout);
    }
    
    public int    getLoadingStatus()
    {
	return av.getLoadingStatus();
    }
    
    public void   setDocumentBase(URL docbase)
    {
	av.setDocumentBase(docbase);
    }

    public void   setWindow(Frame f)
    {
	av.setWindow(f);
    }
    
    public Applet getApplet()
    {
      return av.getApplet();
    }
}

