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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ana Lindstrom-Tamer <Ana.Lindstrom-Tamer@eng.sun.com>
 */



import java.net.URL;
import java.io.*;
import java.util.ResourceBundle;
import java.util.Locale;

import java.awt.*;
import javax.swing.*;

import org.apache.batik.util.*;
import org.apache.batik.dom.svg.*;
import org.apache.batik.swing.JSVGCanvas;
import org.w3c.dom.svg.SVGDocument;

import org.apache.batik.swing.svg.SVGUserAgent;

import org.xml.sax.InputSource;

import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.mozilla.PlugletTagInfo2;


/***********************************************************************/
/************************** Pluglet Factory ****************************/
/***********************************************************************/
public class SVGPlugletFactory implements PlugletFactory {

    /********************************************************************/
    /* main - used for debugging purposes				*/
    /********************************************************************/
    public static void main(String args[]) {
	try {
	    SVGpluglet test = new SVGpluglet();
	    Frame f = new Frame("SVGplugletTest");
	    test.setWindow(f);
	    PlugletStreamListener testStream = test.newStream();
    test.loadDocument("file:///home/lindstro/SVG/batik/1.0/xml-batik/samples/tests/index2.svg");
	} catch (Exception e) {
	    e.printStackTrace();
	}
    }

    /********************************************************************/
    /* constructor							*/
    /********************************************************************/
    public SVGPlugletFactory() {
    }

    /********************************************************************/
    /* PlugletFactory Method:						*/
    /*    Creates a new Pluglet instance based on a MIME type.		*/
    /********************************************************************/
    public Pluglet createPluglet(String mimeType) {
	try {
	    return new SVGpluglet();
	} catch (Exception e) {
	    e.printStackTrace();
	}
	return null;

    }

    /********************************************************************/
    /* PlugletFactory Method:						*/
    /*    Initializes the PlugletFactory instance and is called before
	  any new Pluglet instances are created.			*/
    /********************************************************************/
    public void initialize(PlugletManager manager) {    
    }

    /********************************************************************/
    /* PlugletFactory Method:						*/
    /*    Called when the browser is done with a PlugletFactory
	  instance.							*/
    /********************************************************************/
    public void shutdown() {
    }
}

/***********************************************************************/
/****************************** Pluglet ********************************/
/***********************************************************************/
class SVGpluglet implements Pluglet {

    private Panel panel = null;
    private JSVGCanvas svgCanvas = null;
    private SVGDocument doc = null;
    private URL url = null;

    protected SVGUserAgent userAgent = new UserAgent();

    protected Dimension defaultSize = new Dimension(400,400);
    PlugletPeer peer;
    int w, h;


    /********************************************************************/
    /* constructor							*/
    /********************************************************************/
    public SVGpluglet() {
    }


    /********************************************************************/
    /* Pluglet Method:							*/
    /*    Initializes a newly created Pluglet instance, passing to it
          an instance of PlugletPeer, which it should use for
          communication with the browser.				*/
    /********************************************************************/
    public void initialize(PlugletPeer peer) {
	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	w = info.getWidth();
	h = info.getHeight();
	if (w >= 0 && h >= 0) {
	    defaultSize = new Dimension(w, h);
	}

        this.peer = peer;
    }



    /********************************************************************/
    /* loadDocument							*/
    /********************************************************************/
    public void loadDocument(String url) {

	if (url != null) {
	    try {
		svgCanvas.loadSVGDocument(url);
	    } catch (Exception e) {
		e.printStackTrace();
	    }
	}
    }



    /********************************************************************/
    /* Pluglet Method:							*/
    /*    Called to instruct the Pluglet instance to start.		*/
    /********************************************************************/
    public void start() {
    }

    /********************************************************************/
    /* Pluglet Method:							*/
    /*    Called to instruct the Pluglet instance to stop and suspend
          its state.							*/
    /********************************************************************/
    public void stop() {
    }

    /********************************************************************/
    /* Pluglet Method:							*/
    /*    Called to instruct the Pluglet instance to destroy itself.	*/
    /********************************************************************/
    public void destroy() {
    }

    /********************************************************************/
    /* Pluglet Method:							*/
    /*    This is called to tell the Pluglet instance that the stream
          data for a SRC or DATA attribute (corresponding to an EMBED
	  or OBJECT tag) is ready to be read; it is also called for a
	  full-page Pluglet.						*/
    /********************************************************************/
    public PlugletStreamListener newStream() {
        SVGStreamListener listener = new SVGStreamListener();
        listener.setSVGPluglet(this);
        return listener;
    }



    /********************************************************************/
    /* Pluglet Method:							*/
    /*    Called by the browser to set or change the frame
          containing the Pluglet instance.				*/
    /********************************************************************/
    public void setWindow(Frame frame) {
	if (frame == null) {
	    return;
	}
	if (panel == null) {
	    panel = new Panel();
	}
	if (svgCanvas == null) {
	    svgCanvas = new JSVGCanvas(userAgent, true, true);

	    svgCanvas.setEnableZoomInteractor(true);
	    svgCanvas.setEnableImageZoomInteractor(true);
	    svgCanvas.setEnablePanInteractor(true);
	    svgCanvas.setEnableRotateInteractor(true);
	}

	if (peer != null) {
	    PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	    w = info.getWidth();
	    h = info.getHeight();
	    if (w > 0 && h > 0) {
		defaultSize = new Dimension(w, h);
	    }
	}

	JPanel p = new JPanel(new BorderLayout());
	svgCanvas.setPreferredSize(defaultSize);
	panel.add(p);
	p.add("Center", svgCanvas);
	frame.add(panel);
	frame.pack();
	frame.show();
    }


    /********************************************************************/
    /* Pluglet Method:							*/
    /*    Called to instruct the Pluglet instance to print itself
          to a printer.							*/
    /********************************************************************/
    public void print(java.awt.print.PrinterJob printerJob) {
    }



    /*******************************************************************/
    /************************* UserAgent Class *************************/
    /*******************************************************************/
    /**
     * This class implements a SVG user agent.
     */
    protected class UserAgent implements SVGUserAgent {

        /**
         * Creates a new SVGUserAgent.
         */
        protected UserAgent() {
        }

        /**
         * Displays an error message.
         */
        public void displayError(String message) {
        }

        /**
         * Displays an error resulting from the specified Exception.
         */
        public void displayError(Exception ex) {
	    displayError(ex.getMessage());
        }

        /**
         * Displays a message in the User Agent interface.
         * The given message is typically displayed in a status bar.
         */
        public void displayMessage(String message) {
        }

        /**
         * Returns a customized the pixel to mm factor.
         */
        public float getPixelToMM() {
            return 0.264583333333333333333f; // 96 dpi
        }

        /**
         * Returns the language settings.
         */
        public String getLanguages() {
	    // FOR NOW: just return C
//            return userLanguages;
            return "C";
        }

        /**
         * Returns the user stylesheet uri.
         * @return null if no user style sheet was specified.
         */
        public String getUserStyleSheetURI() {
	    // FOR NOW: just return null
            return null;
        }

        /**
         * Returns the class name of the XML parser.
         */
        public String getXMLParserClassName() {
	    // FOR NOW: hardcode parser class name
//            return application.getXMLParserClassName();
            return "org.apache.crimson.parser.XMLReaderImpl";
        }

        /**
         * Opens a link in a new component.
         * @param uri The document URI.
         * @param newc Whether the link should be activated in a new component.
         */
        public void openLink(String uri, boolean newc) {
	    // FOR NOW: only replace the current image (ie, currently no support
	    // for opening up a new browser or for changing the URL)
//            if (newc) {
//                application.openLink(uri);
//            } else {
                svgCanvas.loadSVGDocument(uri);
//            }
        }

        /**
         * Tells whether the given extension is supported by this
         * user agent.
         */
        public boolean supportExtension(String s) {
            return false;
        }
    }

}


/***********************************************************************/
/*********************** Pluglet Stream Listener ***********************/
/***********************************************************************/
class SVGStreamListener implements PlugletStreamListener {

    SVGpluglet displayer;

    /********************************************************************/
    /* constructor							*/
    /********************************************************************/
    public SVGStreamListener() {
    }

    /********************************************************************/
    /* PlugletStreamListener Method:					*/
    /*    This would be called by the browser to indicate that the URL
	  has started to load.						*/
    /********************************************************************/
    public void onStartBinding(PlugletStreamInfo streamInfo) {
        displayer.loadDocument(streamInfo.getURL());
    }

    /********************************************************************/
    /* setSVGPluglet							*/
    /********************************************************************/
    public void setSVGPluglet(SVGpluglet disp) {
        this.displayer = disp;
    }


    /********************************************************************/
    /* PlugletStreamListener Method:					*/
    /*    This would be called by the browser to indicate that data is
	  available in the input stream.				*/
    /********************************************************************/
    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
    }

    /********************************************************************/
    /* PlugletStreamListener Method:					*/
    /*    This would be called by the browser to indicate the
	  availability of a local file name for the stream data.	*/
    /********************************************************************/
    public void onFileAvailable(PlugletStreamInfo plugletInfo, String fileName) {
    }

    /********************************************************************/
    /* PlugletStreamListener Method:					*/
    /*    This would be called by the browser to indicate that the URL
	  has finished loading.						*/
    /********************************************************************/
    public void onStopBinding(PlugletStreamInfo plugletInfo,int status) {
    }

    /********************************************************************/
    /* PlugletStreamListener Method:					*/
    /*    Returns the type of stream.					*/
    /********************************************************************/
    public int getStreamType() {
        return 1;
    }

}

