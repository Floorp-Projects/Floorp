/*
 * MCP.java
 *
 * Created on February 27, 2007, 10:24 PM
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package org.mozilla.mcp;

import java.awt.BorderLayout;
import java.awt.Frame;
import java.io.FileNotFoundException;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserControlCanvas;
import org.mozilla.webclient.BrowserControlFactory;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.EventRegistration2;
import org.mozilla.webclient.Navigation2;
import org.mozilla.webclient.PageInfoListener;
import org.mozilla.webclient.WebclientEvent;

/**
 *
 * @author edburns
 */
public class MCP {
    
    /** Creates a new instance of MCP */
    public MCP() {
    }
    
    public static final String MCP_LOG = "org.mozilla.mcp";
    public static final String MCP_LOG_STRINGS = "org.mozilla.mcp.MCPLogStrings";

    public static final Logger LOGGER = getLogger(MCP_LOG);
    
    public static Logger getLogger( String loggerName ) {
        return Logger.getLogger(loggerName, MCP_LOG_STRINGS );
    }
    
    private BrowserControl browserControl = null;
    private Navigation2 navigation = null;
    private EventRegistration2 eventRegistration = null;
    private PageInfoListener pageInfoListener = null;
    private Frame frame = null;
    private int x = 0;
    private int y = 0;
    private int width = 1280;
    private int height = 960;
    private boolean initialized = false;
    
    public void setAppData(String absolutePathToNativeBrowserBinDir)
    throws FileNotFoundException,
            ClassNotFoundException {
        BrowserControlFactory.setAppData(absolutePathToNativeBrowserBinDir);
        initialized = true;
    }
    
    void setBounds(int x, int y, int width, int height) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
    }
    
    private Navigation2 getNavigation() {
        if (null == navigation) {
            try {
                navigation = (Navigation2)
                getBrowserControl().queryInterface(BrowserControl.NAVIGATION_NAME);
            }
            catch (Throwable th) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "getNavigation", 
                            th);
                    LOGGER.severe("Unable to obtain Navigation2 reference from BrowserControl");
                }
            }
        }
        return navigation;
    }
    
    private EventRegistration2 getEventRegistration() {
        if (null == eventRegistration) {
            try {
                eventRegistration = (EventRegistration2)
                getBrowserControl().queryInterface(BrowserControl.EVENT_REGISTRATION_NAME);
            }
            catch (Throwable th) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "getEventRegistration", 
                            th);
                    LOGGER.severe("Unable to obtain EventRegistration2 reference from BrowserControl");
                }
            }
            
        }
        return eventRegistration;
    }
    
    private PageInfoListener getPageInfoListener() {
        if (null == pageInfoListener) {
            pageInfoListener = new PageInfoListenerImpl(this);
        }
        return pageInfoListener;
    }
    
    public BrowserControl getBrowserControl() {
        if (!initialized) {
            IllegalStateException ise = new IllegalStateException("Not initialized.  Call setAppData()");
            if (LOGGER.isLoggable(Level.SEVERE)) {
                LOGGER.throwing(this.getClass().getName(), "getBrowserControl", ise);
            }
            throw ise;
        }
        if (null == browserControl) {
            try {
                browserControl = BrowserControlFactory.newBrowserControl();
            } catch (IllegalStateException ex) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "getBrowserControl", ex);
                }
            } catch (InstantiationException ex) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "getBrowserControl", ex);
                }
            } catch (IllegalAccessException ex) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "getBrowserControl", ex);
                }
            }
        }
        return browserControl;
    }
    
    public Frame getRealizedVisibleBrowserWindow() {
        if (null == frame) {
            BrowserControlCanvas canvas = null;
            try {
                canvas = (BrowserControlCanvas) getBrowserControl().queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
            } catch (ClassNotFoundException ex) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "createRealizedVisibleBrowserWindow", ex);
                }
            }
            if (null != canvas) {
                frame = new Frame();
                frame.setUndecorated(true);
                frame.setBounds(x, y, width, height);
                frame.add(canvas, BorderLayout.CENTER);
                frame.setVisible(true);
                canvas.setVisible(true);
                
                getEventRegistration().addDocumentLoadListener(getPageInfoListener());
            }
        }
        return frame;
    }
    
    public void blockingLoad(String url) {
        Navigation2 nav = getNavigation();
        synchronized (this) {
            nav.loadURL(url);
        }
    }
    
    private class PageInfoListenerImpl implements PageInfoListener {
        private MCP owner = null;
        PageInfoListenerImpl(MCP owner) {
            this.owner = owner;
        }

        public void eventDispatched(WebclientEvent webclientEvent) {
            Map<String,Object> eventData = 
                    (Map<String,Object>) webclientEvent.getEventData();
            long type = webclientEvent.getType();
            
            switch ((int)type) {
                case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
                    owner.notifyAll();
                    break;
                default:
                    break;
            }
        }
        
    }
    
}
