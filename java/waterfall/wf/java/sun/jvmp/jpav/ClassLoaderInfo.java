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
 * $Id: ClassLoaderInfo.java,v 1.1 2001/05/09 17:29:59 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav;

/**
 * This class keps track of information about active applet class loaders.
 * The classloaders are identified by their codebase URL.
 *
 * We keep around a pool of recently used class loaders.
 *
 * @author Graham Hamilton
 */

import sun.applet.*;
import java.util.Hashtable;
import java.util.Vector;
import java.net.URL;

class ClassLoaderInfo {
    private URL codebase;
    private int references;
    private Hashtable jars;
    private boolean locked;
    private static boolean initialized;

    // "infos" is a list of ClassLoaderInfos that are available for use.
    private static Hashtable infos = new Hashtable();

    // "zombies" is a list of ClassLoaderInfos that currently have
    // a reference count of zero.  We keep up to zombieLimit of these
    // guys available for resurrection.  The least recently used is
    // at the front, the more recently used at the end.
    // All entries in zombies are also in infos.
    private static int zombieLimit;
    private static Vector zombies = new Vector();

    private static synchronized void initialize() {
	if (initialized) {
	    return;
	}
	initialized = true;
	zombieLimit = Integer.getInteger("javaplugin.jar.cache.size", 100).intValue();
	if (zombieLimit > 0) {
	    System.err.println("java_cache_enabled");
	} else {
	    System.err.println("java_cache_disabled");
	}
    }

    /**
     * Find ClassLoaderInfo for the given AppletPanel.
     *
     * If we don't have any active information, a new ClassLoaderInfo is
     * created with an initial reference count of zero.
     */
    static synchronized ClassLoaderInfo find(AppletPanel panel) {
	initialize();
	URL codebase = panel.getCodeBase();
	//System.err.println("looking classloader for codebase:"+codebase);
	ClassLoaderInfo result = (ClassLoaderInfo)infos.get(codebase);
	if (result == null) {
	    // System.out.println("ClassLoaderCache: adding " + codebase);
	    result = new ClassLoaderInfo(codebase);
	    infos.put(codebase, result);
	} else {
	    // We make sure this loader isn't on the zombies list.
	    zombies.removeElement(result);
	}
	return result;
    }

    /**
     * Add a retaining reference.
     */
    synchronized void addReference() {
	references++;
    }

    /**
     * Remove a retaining reference.  If there are no references left
     * then we put it on the zombies list.
     */
    synchronized void removeReference() {
	references--;
	if (references < 0) {
	    throw new Error("negative ref count???");
	}
	if (references == 0) {
	    addZombie(this);
	}
    }

    /**
     *  Add the given ClassLoaderInfo to the zomboies list.
     * If there are too many zombies we get rid of some.
     */
    private static synchronized void addZombie(ClassLoaderInfo cli) {
	// Add the ClassLoaderInfo to the end of the zombies list.
	zombies.addElement(cli);
	// If there are too many zombies, kill the first one.
	if (zombies.size() > zombieLimit) {
	    ClassLoaderInfo victim = (ClassLoaderInfo)zombies.elementAt(0);
	    // System.out.println("ClassLoaderCache: discarding " + victim.codebase);
	    zombies.removeElementAt(0);
	    infos.remove(victim.codebase);
	    AppletPanel.flushClassLoader(victim.codebase);
	}
    }

    private ClassLoaderInfo(URL codebase) {
	references = 0;
	this.codebase = codebase;
	jars = new Hashtable();
    }

    synchronized void addJar(String name) {
	jars.put(name, name);
    }

    synchronized boolean hasJar(String name) {
	if (jars.get(name) != null) {
	    return true;
	} else {
	    return false;
	}
    }

    /*
    * Flag and utility routines for recording whether local .jar files in lib/app/
    * have been loaded or not.  This is used as a performance optimization
    * so that hasJar() does not need to be checked against the filesystem
    * each time an applet is loaded.
    */
  
    private boolean localJarsLoaded = false;
  
    public boolean getLocalJarsLoaded()  {
        return(localJarsLoaded);
    }
  
    public void setLocalJarsLoaded(boolean loadedFlag)  {
        localJarsLoaded = loadedFlag;
    }
  
    /**
     * Acquire the lock.  This is used to prevent two AppletPanels
     * trying to classload JARs at the same time.
     */
    public final synchronized void lock() throws InterruptedException {
	while (locked) {
	    wait();
	}
	locked = true;
    }

    /**
     * Release the lock.   This allows other people do to classloading.
     */
    public final synchronized void unlock() {
	locked = false;
	notifyAll();
    }
}
