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
 * $Id: ActivatorAppletPanel.java,v 1.1 2001/05/10 18:12:27 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

package sun.jvmp.jpav;
import java.applet.*;
import java.io.*;
import sun.applet.*;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.*;
import java.awt.*;
import sun.jvmp.*;
import sun.jvmp.applet.*;
import java.lang.reflect.*;

public class ActivatorAppletPanel extends AppletPanel implements ProxyType 
{
    
    // Network Access Level
    public static final int NETWORK_ACCESS_NONE = 0;
    public static final int NETWORK_ACCESS_HOST = 1;
    public static final int NETWORK_ACCESS_UNRESTRICTED = 2;

    PluggableJVM m_jvm;

    /*
     * are we initialized
     */
    public static boolean javaEnabled = true;
    private boolean inited = false;

    /**
     * Some constants...
     */
    private static String propertiesFileName = "properties";

    /**
     * Look here for the properties file
     */
    public static File theUserPropertiesFile;
    public static File theAppletViewerPropsFile;

    URL documentURL = null;
    URL baseURL = null;
    Frame  m_f = null;
    private int width;
    private int height;
    
    private static AppletMessageHandler amh = 
	new AppletMessageHandler("appletpanel");

    // Parameters handler
    protected java.util.Hashtable atts;

    // Localization strings.
    private static ResourceBundle rb;
    
    // JAR files that have not yet been loaded.
    private String cookedJars;
    
    public static void loadPropertiesFiles() {

	String sep = File.separator;
	theUserPropertiesFile = new File(System.getProperty("user.home") +
					 sep + ".java" +
					 sep + propertiesFileName);
	// ensure the props folder can be made
	new File(theUserPropertiesFile.getParent()).mkdirs();
	theAppletViewerPropsFile = new File(System.getProperty("java.home") +
					    sep + "lib" +
					    sep + "appletviewer.properties");
    }
        
  
    private class Initer extends Thread {
	ActivatorAppletPanel that;
	
	Initer(ActivatorAppletPanel that) {
	    super(Thread.currentThread().getThreadGroup(), "IniterThread");
	    this.that = that;
  	}

	/**
         * Main body of thread.  Call init on our AppletViewer.
	 */
	public void run() {
	    init_run_wrapper();	// To identify this thread's stack
	}
	
	private void init_run_wrapper() 
	{
	    m_jvm.trace("real Init()", PluggableJVM.LOG_DEBUG);
	    that.init();
	    that.sendEvent(sun.applet.AppletPanel.APPLET_INIT);
	    that.sendEvent(sun.applet.AppletPanel.APPLET_START);
	}
    }

    /**
     * Construct a new applet viewer.
     * Restricted to subclasses for security reasons,
     * 
     * @param appletContext the AppletContext to use
     */
    public  ActivatorAppletPanel(PluggableJVM jvm, 
				 WFAppletContext appletContext,
				 Hashtable atts) {
	if (appletContext == null) 
	    throw new IllegalArgumentException("AppletContext");
	this.appletContext = appletContext;
        appletContext.addAppletInContext(this);
	this.atts = atts;
	m_jvm = jvm;
    }

    public void sendEvent(int id) {
      super.sendEvent(id);
    }

    /**
     * Init the applet, called once to start the download of the applet 
     */
    public void init() {
	ClassLoaderInfo cli = ClassLoaderInfo.find(this);
	cli.addReference();
	synchronized(AppletViewer.class) {
	    super.init();
	}
	sendEvent(sun.applet.AppletPanel.APPLET_LOAD);
	//sendEvent(sun.applet.AppletPanel.APPLET_INIT);
    }

    /**
     * Start the applet.
     */
    public void appletStart() {
	if (inited)
	    sendEvent(sun.applet.AppletPanel.APPLET_START);
    }

    /**
     * Stop the applet.
     */
    public void appletStop() {
	sendEvent(sun.applet.AppletPanel.APPLET_STOP);
    }
    
    public int getLoadingStatus()
    {
        return status;
    }

    /**
     * Notification that the applet is being closed
     *
     * @param timeOut max time we are waiting for the applet to die
     *		in milliseconds.
     */ 
    public void onClose(final int timeOut) {

    	// Just make sure we are destroying the thread from a utility
	// thread to not block the main thread for that.

	Runnable work = new Runnable() {
	    public void run() {
		onPrivateClose(timeOut);	
	    }
	    };
	Thread closingThread = new Thread(work);
	closingThread.start();
    }
    
    /**
     * Notification that the applet is being closed
     *
     * @param timeOut max time we are waiting for the applet to die
     *		in milliseconds.
     */
    protected void onPrivateClose(int timeOut) {
	appletContext.removeAppletFromContext(this);
	if (status==APPLET_LOAD) {
	    stopLoading();
	}
	sendEvent(APPLET_STOP);
        sendEvent(APPLET_DESTROY);
        sendEvent(APPLET_DISPOSE);
        sendEvent(APPLET_QUIT);
	ClassLoaderInfo cli = ClassLoaderInfo.find(this);
	cli.removeReference();
    }
    
    /**
     * Initialized the properties for this AppletViewer like the Applet
     * codebase and document base.
     */
    protected void initProperties() {
	String att = getParameter("java_codebase");
	if (att == null)
	    att = getParameter("codebase");
	if (att != null) {
	    if (!att.endsWith("/")) {
		att += "/";
	    }
	    try {
		baseURL = new URL(documentURL, att);
	    } catch (MalformedURLException e) {
	    }
	}
	if (baseURL == null) {
	    String file = documentURL.getFile();
	    int i = file.lastIndexOf('/');
	    if (i > 0 && i < file.length() - 1) {
		try {
		    baseURL = new URL(documentURL, file.substring(0, i + 1));
		} catch (MalformedURLException e) {
		}
	    }
	}
	
	// when all is said & done, baseURL shouldn't be null
	if (baseURL == null)
	    baseURL = documentURL;
    }

    /**
     * Get an applet parameter.
     */
    public String getParameter(String name) {
	name = name.toLowerCase();
	if (atts.get(name) != null) {
	    return (String) atts.get(name);
	} else {
	    String value=null;
	    try {
    		value = getParameterFromHTML(name);
	    } catch (Throwable e) {
		m_jvm.trace(e, PluggableJVM.LOG_WARNING);
	    }
	    if (value != null)
    	        atts.put(name.toLowerCase(), value);
	    return value;
	}	   
    }


    /**
     * Get applet's parameters.
     */
    public Hashtable getParameters()
    {
	return (Hashtable) atts.clone();
    }


    /**
     * Method to retrieve the parameter from the HTML tags
     * 
     * @param name the parameter name
     *
     * @return the parameter value, null if undefined
     */
    protected String getParameterFromHTML(String name)
    {
	return null;
    }

    /**
     * Set an applet parameter.
     */
    public void setParameter(String name, Object value) {
	name = name.toLowerCase();
	atts.put(name, value.toString());
    }

    /**
     * Get the document url.
     */
    public URL getDocumentBase() {
	return documentURL;

    }

    public void setWindow(Frame f)
    {
	if (f == null)
	  {
	    m_jvm.trace("Got zero Frame for SetWindow", 
			PluggableJVM.LOG_ERROR);
	    return;
	  }	
	f.setLayout(null);
	Applet a = getApplet();
	if (a == null) {
	    try {
		int wd = Integer.parseInt(getParameter("width"));
		int ht = Integer.parseInt(getParameter("height"));
		this.width = wd;
		this.height = ht;
	    } catch (NumberFormatException e) {
		// Try and maintain consistency between the parameters and
		// our width/height. If the parameters are not properly
		// set to be integers, then reset them to our w, h
		setParameter("width", new Integer(width));
		setParameter("height", new Integer(height));
		this.width = width;
		this.height = height;
	    }
	} else {
	    // The applet exists, so resize it and its frame, and
	    // have the width/height parameters reflect these values
	    setParameter("width", new Integer(width));
	    setParameter("height", new Integer(height));
	    this.width = width;
	    this.height = height;
	    
	    setSize(width, height);
	    a.resize(width, height);
	    a.setVisible(true);
	}
	setBounds(0, 0, width, height);
	if (m_f == f) return;
	m_f = f;
	f.add(this);
	f.show();
	f.setBounds(0, 0, width, height);
	maybeInit();
    }
    
    private void maybeInit() {
	m_jvm.trace("maybeInit()", PluggableJVM.LOG_DEBUG);
	if (!inited && m_f != null && getDocumentBase() != null) 
	    {
		inited = true;
		Initer initer = new Initer(this);
		initer.start();
	    }
    }

    /**
     * Set the document url.
     * This must be done early, before initProperties is called.
     */
    public void setDocumentBase(URL url) 
    {
	documentURL = url;
	initProperties();
	// If the window has already been set, we can init the applet.
	maybeInit();
    }

    /**
     * Get the base url.
     */
    public URL getCodeBase() {
	return baseURL;
    }
    /**
     * Get the width.
     */
    public int getWidth() {
	String w = getParameter("width");
	if (w != null) {
	    return Integer.valueOf(w).intValue();
	}
	return 0;
    }

    /**
     * Get the height.
     */
    public int getHeight() {
	String h = getParameter("height");
	if (h != null) {
	    return Integer.valueOf(h).intValue();
	} 
	return 0;
    }

    /**
     * Get the code parameter
     */
    public String getCode() {

	// Support HTML 4.0 style of OBJECT tag.
	//
	// <OBJECT classid=java:sun.plugin.MyClass .....>
	// <PARAM ....>
	// </OBJECT>
	// 
	// In this case, the CODE will be inside the classid
	// attribute.
	//
	String moniker = getParameter("classid");
	String code = null;

	if (moniker != null)
	{
	    int index = moniker.indexOf("java:");

	    if (index > -1)
	    {
		code = moniker.substring(5 + index);

		if (code != null || !code.equals(""))
		    return code;
	    }
	}

	code = getParameter("java_code");
	if (code==null)
	    code=getParameter("code");
	return code;
    }

    /**
     * Return the list of jar files if specified.
     * Otherwise return null.
     */
    public String getJarFiles() {
	return cookedJars;
    }


   /*
    *  Allow pre-loading of local .jar files in plug-in lib/app directory
    *  These .jar files are loaded with the PluginClassLoader so they
    *  run in the applet's sandbox thereby saving developers the trouble
    *  of writing trusted support classes.
    *  The ClassLoaderInfo cli should be locked.
    */
    protected Vector getLocalJarFiles()  {
    
        String fSep = File.separator;
        String libJarPath = 
	    System.getProperty("java.home") + fSep + "lib";
	    			
        String appJarPath = libJarPath + fSep + "applet";
        return(getJarFilesFromPath(appJarPath));
	
    }

        
    private Vector getJarFilesFromPath (String basePath)  {
	File dir = new File(basePath);
	if (!dir.exists())  {
	    return(null);
	}
    
	String[] jarList = dir.list(new FilenameFilter()  {
	    public boolean accept(File f, String s)  {
	        return(s.endsWith(".jar"));
	    }
	});
	
	Vector jars = new Vector();
	
        String fSep = File.separator;
        for (int i = 0; i < jarList.length; i++)  {
	    String fullJarPath = basePath + fSep + jarList[i];
	    jars.add(new File(fullJarPath));
	}
	
	return(jars);
    }    

  
   
  /**
   * Return the value of the object param
   */
  public String getSerializedObject() {
    String object = getParameter("java_object");
    if (object==null) 
      object=getParameter("object");// another name?
    return object;
  }
    
/**
 * Get the applet context. For now this is
 * also implemented by the AppletPanel class.
 */
    public AppletContext getAppletContext() 
    {
	return appletContext; 
    }

    public Object getViewedObject() 
    {
	Applet applet = super.getApplet();
	return applet;
    }

    
    /**
     * Paint this panel while visible and loading an applet to entertain
     * the user.
     *
     * @param g the graphics context
     */
    public void paint(Graphics g)
    {
    	if (getViewedObject() == null && g != null) {
            setBackground(Color.lightGray);
      	    g.setColor(Color.black);
            Dimension d = getSize();
            Font fontOld = g.getFont();
            FontMetrics fm = null;
            if (fontOld != null)
                fm = g.getFontMetrics(fontOld);
            String str = getWaitingMessage();

            // Display message on the screen if the applet is not started yet.
            if (d != null && fm != null)
                g.drawString(str, (d.width - fm.stringWidth(str))/ 2,
                             (d.height + fm.getAscent())/ 2);
    	}
    }

    public String getWaitingMessage() {
	return "loading applet...";
	//return getMessage("loading") + getHandledType() + " ...";
    }

    /*
     * <p>
     * Load an applet from a serialized stream. This is likely to happen
     * when the user uses the Back/Forward buttons
     * </p>
     * 
     * @param is Input stream of the serialized applet
     */
    protected void load(java.io.InputStream is) {
	this.is = is;
    }

   
    /*
     * @return the applet name
     */
    public String getName()  {

	String name = getParameter("name");
	if (name!=null)
	    return name;

        // Remove .class extension
        name = getCode();
        if (name != null){
            int index = name.lastIndexOf(".class");
            if (index != -1)
                name = name.substring(0, index);
        } else{
            // Remove .object extension
            name = getSerializedObject();

            if (name != null) {
                int index = name.lastIndexOf(".ser");
                if (index != -1)
                    name = name.substring(0, index);
            }
        }

        return name;
    }


    /**
     * @return the java component displayed by this viewer class
     */
    protected String getHandledType() {
	return getMessage("java_applet");
    }

    public void setStatus(int status) {
	this.status = status;
    }

    public void showActivatorAppletStatus(String msg) {
	showAppletStatus(msg);
    }

    public void showActivatorAppletLog(String msg) {
	showAppletLog(msg);
    }

    public void setDoInit(boolean doInit) {
	this.doInit = doInit;
    }

    private WFAppletContext appletContext;


    /**
     * Method to get an internationalized string from the Activator resource.
     */
    public static String getMessage(String key) {
      return key;
    }


    /**
     * Method to get an internationalized string from the Activator resource.
     */
    public static String[] getMessageArray(String key) {
      return new String[] { key };
    }

    private java.io.InputStream is;

    /**
     * This method actually creates an AppletClassLoader.
     *
     * It can be override by subclasses (such as the Plug-in)
     * to provide different classloaders. This method should be
     * called only when running inside JDK 1.2.
     */    
    protected AppletClassLoader createClassLoader(final URL codebase) {
        return new ActivatorClassLoader(codebase);
    }
    
     /*
     * We overload our parent loadJarFiles so tht we can avoid
     * reloading JAR files that are already loaded.
     *						KGH Mar 98
     */
    protected void loadJarFiles(AppletClassLoader loader)
	throws IOException, InterruptedException 
    {
	
	
        // Cache option as read from the Html page tag
        String copt;
      
        // Cache Archive value as read from the Html page tag
        String carch;

        // Cache Version value as read from the Html page tag
        String cver;
      
        // Vector to store the names of the jar files to be cached
        Vector jname = new Vector();

        // Vector to store actual version numbers
        Vector jVersion = new Vector();
      

        // Figure out the list of all required JARs.
        String raw = getParameter("java_archive");
        if (raw == null) 
        {
	    raw = getParameter("archive");
        }
      
      
        ClassLoaderInfo cli = ClassLoaderInfo.find(this);
      
        try 
        {
	    // Prevent two applets trying to load JARS from the same 
	    // classloader at the same time.
	    cli.lock();
	
	    // If there are no JARs, this is easy.
	    if (raw == null) 
            {
	        return;
	    } 
      
	    // Figure out which JAR files still need to be loaded.
	    String cooked = "";
	    StringTokenizer st = new StringTokenizer(raw, ",", false);
	    while(st.hasMoreTokens()) 
            {
	        String tok = st.nextToken().trim();
	        if (cli.hasJar(tok)) 
                {
	            continue;
	        }
	        
                cli.addJar(tok);
	        
                if (cooked.equals("")) 
                {
	            cooked = tok;
	        } 
                else 
                {
	            cooked = cooked + "," + tok;
	        }
	    }
	                
            cookedJars = cooked;
	
	    // Now call into our superlcass to do the actual JAR loading.
	    // It will call back to our getJarFiles method to find which
	    // JARs need to be loaded, and we will give it the cooked list.
	    super.loadJarFiles(loader);
	
        } 
        finally 
        {
	    // Other people can load JARs now.
	    cli.unlock();
        }
    }	
    
    
};






