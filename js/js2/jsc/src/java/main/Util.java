/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ResourceBundle;
import java.text.MessageFormat;
import java.util.MissingResourceException;

/*
 * Utility for the main driver.
 */
public class Util {

    /*
     * Help for verbosity.
     */
    public static boolean verbose = false;

    public static void log(String s) {
	System.out.println(s);
    }


    /* 
     * Help for loading localized messages.
     */
    private static ResourceBundle m;

    private static void initMessages() {
	try {
	    m=ResourceBundle.getBundle("com.sun.tools.javah.resources.l10n");
	} catch (MissingResourceException mre) {
	    fatal("Error loading resources.  Please file a bug report.", mre);
	}
    }

    public static String getText(String key) {
	return getText(key, null, null);
    }

    private static String getText(String key, String a1, String a2){
	if (m == null)
	    initMessages();
	try {
	    return MessageFormat.format(m.getString(key),
					new String[] { a1, a2 });
	} catch (MissingResourceException e) {
	    fatal("Key " + key + " not found in resources.", e);
	}
	return null; /* dead code */
    }

    /*
     * Usage message.
     */
    public static void usage(int exitValue) {
	if (exitValue == 0) {
	    System.out.println(getText("usage"));
	} else {
	    System.err.println(getText("usage"));
	}
	System.exit(exitValue);
    }

    public static void version() {
	System.out.println(getText("javah.version",
				   System.getProperty("java.version"), null));
	System.exit(0);
    }

    /*
     * Failure modes.
     */
    public static void bug(String key) {
	bug(key, null);
    }
    
    public static void bug(String key, Exception e) {
	if (e != null)
	    e.printStackTrace();
	System.err.println(getText(key));
	System.err.println(getText("bug.report"));
	System.exit(11);
    }

    public static void error(String key) {
	error(key, null);
    }

    public static void error(String key, String a1) {
	error(key, a1, null);
    }

    public static void error(String key, String a1, String a2) {
	error(key, a1, a2, false);
    }
    
    public static void error(String key, String a1, String a2, 
			     boolean showUsage) {
	System.err.println("Error: " + getText(key, a1, a2));
	if (showUsage)
	    usage(15);
	System.exit(15);
    }


    private static void fatal(String msg) {
	fatal(msg, null);
    }
    
    private static void fatal(String msg, Exception e) {
	if (e != null) {
	    e.printStackTrace();
	}
	System.err.println(msg);
	System.exit(10);
    }

    /*
     * Support for platform specific things in javah, such as pragma
     * directives, exported symbols etc.
     */
    static ResourceBundle platform = null;

    static String getPlatformString(String key) {
	if (platform == null)
	    initPlatform();
	try {
	    return platform.getString(key);
	} catch (MissingResourceException mre) {
	    return null;
	}
    }
    
    private static void initPlatform() {
	String os = System.getProperty("os.name");
	if (os.startsWith("Windows")) 
	    os = "win32";
	String arch = System.getProperty("os.arch");
	String resname = "com.sun.tools.javah.resources." + os + "_" + arch;
	try {
	    platform=ResourceBundle.getBundle(resname);
	} catch (MissingResourceException mre) {
	    fatal("Error loading resources.  Please file a bug report.", mre);
	}
    }
}

/*
 * The end.
 */
