/*
 * $Id: MCP.java,v 1.6 2007/03/09 04:34:24 edburns%acm.org Exp $
 */

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */
package org.mozilla.mcp;

import java.awt.AWTException;
import java.awt.BorderLayout;
import java.awt.Frame;
import java.awt.Robot;
import java.awt.event.InputEvent;
import java.awt.event.KeyListener;
import java.awt.event.MouseListener;
import java.io.FileNotFoundException;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.mozilla.dom.util.DOMTreeDumper;
import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BrowserControlCanvas;
import org.mozilla.webclient.BrowserControlFactory;
import org.mozilla.webclient.CurrentPage2;
import org.mozilla.webclient.DocumentLoadEvent;
import org.mozilla.webclient.EventRegistration2;
import org.mozilla.webclient.Navigation2;
import org.mozilla.webclient.PageInfoListener;
import org.mozilla.webclient.WebclientEvent;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

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
    
    static Logger getLogger( String loggerName ) {
        return Logger.getLogger(loggerName, MCP_LOG_STRINGS );
    }
    
    private BrowserControl browserControl = null;
    private Navigation2 navigation = null;
    private EventRegistration2 eventRegistration = null;
    private BrowserControlCanvas canvas = null;
    private PageInfoListener pageInfoListener = null;
    private Frame frame = null;
    private int x = 0;
    private int y = 0;
    private int width = 1280;
    private int height = 960;
    private boolean initialized = false;
    private Robot robot;
    private DOMTreeDumper treeDumper = null;
    
    public void setAppData(String absolutePathToNativeBrowserBinDir)
    throws FileNotFoundException,
            ClassNotFoundException {
        BrowserControlFactory.setAppData(absolutePathToNativeBrowserBinDir);
        initialized = true;
    }
    
    private DOMTreeDumper getDOMTreeDumper() {
        if (null == treeDumper) {
            treeDumper = new DOMTreeDumper("MCP", false);
        }
        return treeDumper;
    }
    
    public void setBounds(int x, int y, int width, int height) {
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

    private BrowserControlCanvas getBrowserControlCanvas() {
        if (null == canvas) {
            try {
                canvas = (BrowserControlCanvas)
                getBrowserControl().queryInterface(BrowserControl.BROWSER_CONTROL_CANVAS_NAME);
            }
            catch (Throwable th) {
                if (LOGGER.isLoggable(Level.SEVERE)) {
                    LOGGER.throwing(this.getClass().getName(), "getBrowserControlCanvas", 
                            th);
                    LOGGER.severe("Unable to obtain BrowserControlCanvas reference from BrowserControl");
                }
            }
            
        }
        return canvas;
    }
    
    private CurrentPage2 getCurrentPage() {
        CurrentPage2 currentPage = null;

        try {
            currentPage = (CurrentPage2)
            getBrowserControl().queryInterface(BrowserControl.CURRENT_PAGE_NAME);
        }
        catch (Throwable th) {
            if (LOGGER.isLoggable(Level.SEVERE)) {
                LOGGER.throwing(this.getClass().getName(), "getCurrentPage",
                        th);
                LOGGER.severe("Unable to obtain CurrentPage2 reference from BrowserControl");
            }
        }

        return currentPage;
    }
    
    private PageInfoListener getPageInfoListener() {
        if (null == pageInfoListener) {
            pageInfoListener = new PageInfoListenerImpl(this);
        }
        return pageInfoListener;
    }
    
    public void addAjaxListener(AjaxListener listener) {
        getEventRegistration().addDocumentLoadListener(listener);
    }
    
    public void removeAjaxListener(AjaxListener listener) {
        getEventRegistration().removeDocumentLoadListener(listener);
    }

    public void addMouseListener(MouseListener listener) {
        getBrowserControlCanvas().addMouseListener(listener);
    }
    
    public void removeMouseListener(MouseListener listener) {
        getBrowserControlCanvas().removeMouseListener(listener);
    }

    public void addKeyListener(KeyListener listener) {
        getBrowserControlCanvas().addKeyListener(listener);
    }
    
    public void removeKeyListener(KeyListener listener) {
        getBrowserControlCanvas().removeKeyListener(listener);
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
    
    public void deleteBrowserControl() {
        getRealizedVisibleBrowserWindow().setVisible(false);
        BrowserControlFactory.deleteBrowserControl(getBrowserControl());
    }
    
    public Element findElement(String id) {
        Element result = null;
        Document dom = getCurrentPage().getDOM();
        try {
            result = dom.getElementById(id);
        }
        catch (Exception e) {
            
        }
        if (null == result) {
            result = getDOMTreeDumper().findFirstElementWithName(dom, id);
        }
        
        return result;
    }
    
    public boolean findInPage(String toFind) {
        boolean found = false;
        found = getCurrentPage().find(toFind, true, true);
        return found;
    }
    
    public void clickElement(String id) {
        Element element = findElement(id);
        String clientX = null, clientY = null;
        if (null != element) {
            clientX = element.getAttribute("clientX");
            clientY = element.getAttribute("clientY");
            int x,y;
            if (null != clientX && null != clientY) {
                try {
                    x = Integer.valueOf(clientX).intValue();
                    y = Integer.valueOf(clientY).intValue();
                    Robot robot = getRobot();
                    robot.mouseMove(x, y);
                    robot.mousePress(InputEvent.BUTTON1_MASK);
                    robot.mouseRelease(InputEvent.BUTTON1_MASK);

                } catch (NumberFormatException ex) {
                    LOGGER.throwing(this.getClass().getName(), "clickElementGivenId",
                        ex);
                    ex.printStackTrace();
                    throw new IllegalStateException(ex);
                }
            }
        }
        if (null == element || null == clientX || null == clientY) {
            throw new IllegalStateException("Unable to click element " + id);
        }
    }
    
    public void blockingClickElement(String idOrName) {
        synchronized (this) {
            try {
                clickElement(idOrName);
                this.wait();
            }
            catch (IllegalStateException ise) {
                LOGGER.throwing(this.getClass().getName(), "blockingClickElementGivenId",
                        ise);
            }
            catch (InterruptedException ie) {
                LOGGER.throwing(this.getClass().getName(), "blockingClickElementGivenId",
                        ie);
            }
        }
    }
    
    public void blockingLoad(String url) {
        Navigation2 nav = getNavigation();
        synchronized (this) {
            nav.loadURL(url);
            try {
                this.wait();
            } catch (InterruptedException ex) {
                LOGGER.throwing(this.getClass().getName(), "blockingLoad",
                        ex);
                ex.printStackTrace();
            }
        }
    }
    
    private Robot getRobot() {
        if (null == robot) {
            try {
                robot = new Robot();
            } catch (AWTException ex) {
                LOGGER.throwing(this.getClass().getName(), "getRobot",
                        ex);
                ex.printStackTrace();
            }
        }
        return robot;
    }
    
    private class PageInfoListenerImpl implements PageInfoListener {
        private MCP owner = null;
        PageInfoListenerImpl(MCP owner) {
            this.owner = owner;
        }

        public void eventDispatched(WebclientEvent webclientEvent) {
            boolean logInfo = LOGGER.isLoggable(Level.INFO);
            Map eventData = 
                    (Map) webclientEvent.getEventData();
            long type = webclientEvent.getType();
            
            switch ((int)type) {
                case ((int) DocumentLoadEvent.END_AJAX_EVENT_MASK):
                case ((int) DocumentLoadEvent.END_DOCUMENT_LOAD_EVENT_MASK):
                    synchronized (owner) {
                        owner.notifyAll();
                    }
                    break;
                case ((int) DocumentLoadEvent.START_URL_LOAD_EVENT_MASK):
                    String method = (String) eventData.get("method");
                    method = (null != method) ? method : "no method given";
                    String uri = (String) eventData.get("URI");
                    uri = (null != uri) ? uri : "no URI given";
                    Map<String,String> requestHeaders = (Map<String,String>)
                        eventData.get("headers");
                    StringBuffer headerValues = null;
                    if (null != requestHeaders) {
                        headerValues = new StringBuffer();
                        for (String cur : requestHeaders.keySet()) {
                            headerValues.append(cur + ": " + 
                                    requestHeaders.get(cur) + "\n");
                        }
                    }
                    headerValues = (null != headerValues) ? headerValues :
                        new StringBuffer("no headers given: ");
                    if (logInfo) {
                        LOGGER.info("HTTP REQUEST:\n" + 
                                method + " " + uri + "\n" + 
                                headerValues);
                    }
                    break;
                default:
                    break;
            }
        }
        
    }
    
}
