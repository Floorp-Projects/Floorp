/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * $Id: AppletMessageHandler.java,v 1.1 2001/05/09 17:29:59 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav;

import java.util.ResourceBundle;
import java.util.MissingResourceException;
import java.text.MessageFormat;

/**
 * An hanlder of localized messages.
 *
 * @version     1.8, 03 Mar 1997
 * @author      Koji Uno
 */
public class AppletMessageHandler {
    private static ResourceBundle rb;
    private String baseKey = null;

    static {
        try {
            rb = ResourceBundle.getBundle
		("sun.jvmp.resources.MsgAppletViewer");
        } catch (MissingResourceException e) {
            //System.err.println(e.getMessage());
            //System.exit(1);
        }
    };

    public AppletMessageHandler(String baseKey) {
        this.baseKey = baseKey;
    }

    public String getMessage(String key) {
        return (String)rb.getString(getQualifiedKey(key));
    }

    public String getMessage(String key, Object arg){
        String basemsgfmt = (String)rb.getString(getQualifiedKey(key));
        MessageFormat msgfmt = new MessageFormat(basemsgfmt);
        Object msgobj[] = new Object[1];
	if (arg == null) arg = "null";
	msgobj[0] = arg;
	return msgfmt.format(msgobj);
    }

    public String getMessage(String key, Object arg1, Object arg2) {
        String basemsgfmt = (String)rb.getString(getQualifiedKey(key));
        MessageFormat msgfmt = new MessageFormat(basemsgfmt);
        Object msgobj[] = new Object[2];
	if (arg1 == null) {
	    arg1 = "null";
	}
	if (arg2 == null) {
	    arg2 = "null";
	}
	msgobj[0] = arg1;
	msgobj[1] = arg2;
	return msgfmt.format(msgobj);
    }

    public String getMessage(String key, Object arg1, Object arg2, Object arg3) {
        String basemsgfmt = (String)rb.getString(getQualifiedKey(key));
        MessageFormat msgfmt = new MessageFormat(basemsgfmt);
        Object msgobj[] = new Object[3];
	if (arg1 == null) {
	    arg1 = "null";
	}
	if (arg2 == null) {
	    arg2 = "null";
	}
	if (arg3 == null) {
	    arg3 = "null";
	}
	msgobj[0] = arg1;
	msgobj[1] = arg2;
	msgobj[2] = arg3;
	return msgfmt.format(msgobj);
    }

    public String getMessage(String key, Object arg[]) {
        String basemsgfmt = (String)rb.getString(getQualifiedKey(key));
        MessageFormat msgfmt = new MessageFormat(basemsgfmt);
        return msgfmt.format(arg);
    }

    String getQualifiedKey(String subKey) {
        return baseKey + "." + subKey;
    }
}
