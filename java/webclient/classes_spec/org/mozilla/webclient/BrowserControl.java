/* 
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 */

package org.mozilla.webclient;


// BrowserControl.java

/**
 *
 * <p>Provides a common access point for other webclient interfaces.
 * There is one instance of this class per application-level browser
 * window.  Instances must be created using {@link
 * BrowserControlFactory}.</p>
 *
 * @version $Id: BrowserControl.java,v 1.5 2005/03/15 02:49:16 edburns%acm.org Exp $
 * 
 * @see org.mozilla.webclient.BrowserControlFactory
 * 
 */

public interface BrowserControl {
    
    /**
     * <p>The name of the interface for the {@link Bookmarks} API.</p>
     */
    
    public static String BOOKMARKS_NAME = "webclient.Bookmarks";
    
    /**
     * <p>The name of the interface for the {@link BrowserControlCanvas}
     * that must be added to the application to show the browser
     * window.</p>
     */
    
    public static String BROWSER_CONTROL_CANVAS_NAME = "webclient.BrowserControlCanvas";

    /**
     * <p>The name of the interface for the {@link CurrentPage} API.</p>
     */

    public static String CURRENT_PAGE_NAME = "webclient.CurrentPage";

    /**
     * <p>The name of the interface for the {@link EventRegistration}
     * API.</p>
     */
    public static String EVENT_REGISTRATION_NAME = "webclient.EventRegistration";

    /**
     * <p>The name of the interface for the {@link EventRegistration2}
     * API.</p>
     */

    public static String EXTENDED_EVENT_REGISTRATION_NAME = "webclient.ExtendedEventRegistration";

    /**
     * <p>The name of the interface for the {@link History} API.</p>
     */

    public static String HISTORY_NAME = "webclient.History";

    /**
     * <p>The name of the interface for the {@link Navigation} API.</p>
     */

    public static String NAVIGATION_NAME = "webclient.Navigation";

    /**
     * <p>The name of the interface for the cache management API.
     * Unimplemented.</p>
     */

    public static String CACHE_MANAGER_NAME = "webclient.cache.NetDataCacheManager";
    
    /**
     * <p>The name of the interface for the {@link Preferences} API.</p>
     */

    public static String PREFERENCES_NAME = "webclient.Preferences";

    /**
     * <p>The name of the interface for the Print API.
     * Unimplemented.</p>
     */

    public static String PRINT_NAME = "webclient.Print";

    /**
     * <p>The name of the interface for the {@link WindowControl} API.</p>
     */

    public static String WINDOW_CONTROL_NAME = "webclient.WindowControl";

    /**
     * <p>The name of the interface for the {@link ProfileManager} API.</p>
     */

    public static String PROFILE_MANAGER_NAME = "webclient.ProfileManager";

    /**
     * @deprecated the type of browser implementation is derived using a
     * services definition loaded from {@link
     * org.mozilla.util.Utilities#getImplFromServices}
     *
     */
    
    public static String BROWSER_TYPE_NATIVE = null;
    public static String BROWSER_TYPE_NON_NATIVE = null;
    

    /**
     *
     * <p>Obtain a reference to a class implementing the argument
     * <code>interfaceName</code>.</p>
     *
     * @param interfaceName valid values for <code>interfaceName</code>
     * are any of the *_NAME class variables in this interface.
     *
     * @throws ClassNotFoundException if the <code>interfaceName</code>
     * can't be instantiated for whatever reason.
     *
     */
    
    public Object queryInterface(String interfaceName) throws ClassNotFoundException;

} // end of interface BrowserControl
