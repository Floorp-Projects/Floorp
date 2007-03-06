/* 
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
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.impl.wrapper_native;

import java.awt.HeadlessException;
import java.awt.Toolkit;
import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.ClipboardOwner;
import java.awt.datatransfer.StringSelection;
import java.awt.datatransfer.Transferable;
import org.mozilla.util.Assert;
import org.mozilla.util.ParameterCheck;

import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.CurrentPage2;
import org.mozilla.webclient.Selection;
import org.mozilla.webclient.impl.WrapperFactory;

import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.w3c.dom.Document;
import org.w3c.dom.Node;

import org.mozilla.webclient.UnimplementedException;

import org.mozilla.dom.DOMAccessor;
import org.mozilla.dom.util.DOMTreeDumper;
import org.mozilla.util.Log;

public class CurrentPageImpl extends ImplObjectNative implements CurrentPage2, 
        ClipboardOwner
{
//
// Protected Constants
//

//
// Class Variables
//
    
    public static final String LOG = "org.mozilla.impl.wrapper_native.CurrentPageImpl";

    public static final Logger LOGGER = Log.getLogger(LOG);
    

private static boolean domInitialized = false;

//
// Instance Variables
//

// Attribute Instance Variables


// Relationship Instance Variables

    private DOMTreeDumper domDumper = null;

//
// Constructors and Initializers
//

public CurrentPageImpl(WrapperFactory yourFactory,
                       BrowserControl yourBrowserControl)
{
    super(yourFactory, yourBrowserControl);
    // force the class to be loaded, thus loading the JNI library
    if (!domInitialized) {
        DOMAccessor.initialize();
    }
    domDumper = new DOMTreeDumper();
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from CurrentPage
//

public void copyCurrentSelectionToSystemClipboard()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    plainTextSelection = null;
    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
        public Object run() {
            nativeCopyCurrentSelectionToSystemClipboard(CurrentPageImpl.this.getNativeBrowserControl());
            return null;
        }
        public String toString() {
            return "WCRunnable.nativeCopyCurrentSelectionToSystemClipboard";
        }
    });
    if (null != plainTextSelection) {
        try {
            Clipboard clip = Toolkit.getDefaultToolkit().getSystemClipboard();
            clip.setContents(plainTextSelection, this);
        } catch (IllegalStateException ise) {
            if (LOGGER.isLoggable(Level.WARNING)) {
                LOGGER.warning("Unable to set transferable to clipboard: " +
                        plainTextSelection.toString());
            }
        } catch (HeadlessException he) {
            if (LOGGER.isLoggable(Level.SEVERE)) {
                LOGGER.severe("Unable to set clipboard contents because GraphicsEnvironment is Headless: "+
                        he.getMessage());
            }
        }
        finally {
            plainTextSelection = null;
        }
        
    }
}

public void copyCurrentSelectionHtmlToSystemClipboard()
{
    getWrapperFactory().verifyInitialized();
    Assert.assert_it(-1 != getNativeBrowserControl());

    htmlSelection = null;
    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
        public Object run() {
            nativeCopyCurrentSelectionToSystemClipboard(CurrentPageImpl.this.getNativeBrowserControl());
            return null;
        }
        public String toString() {
            return "WCRunnable.nativeCopyCurrentSelectionHtmlToSystemClipboard";
        }
    });
    if (null != htmlSelection) {
        try {
            Clipboard clip = Toolkit.getDefaultToolkit().getSystemClipboard();
            clip.setContents(htmlSelection, this);
        } catch (IllegalStateException ise) {
            if (LOGGER.isLoggable(Level.WARNING)) {
                LOGGER.warning("Unable to set transferable to clipboard: " +
                        htmlSelection.toString());
            }
        } catch (HeadlessException he) {
            if (LOGGER.isLoggable(Level.SEVERE)) {
                LOGGER.severe("Unable to set clipboard contents because GraphicsEnvironment is Headless: "+
                        he.getMessage());
            }
        }
        finally {
            htmlSelection = null;
        }
        
    }
}

public Selection getSelection() {
    getWrapperFactory().verifyInitialized();
    final Selection selection = new SelectionImpl();

    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
	     nativeGetSelection(CurrentPageImpl.this.getNativeBrowserControl(),
				selection);
	     return null;
	    }
            public String toString() {
                return "WCRunnable.nativeGetSelection";
            }

	});
    
    return selection;
}

public void highlightSelection(Selection selection) {
    if (selection != null && selection.isValid()) {
        final Node startContainer = selection.getStartContainer();
        final Node endContainer = selection.getEndContainer();
        final int startOffset = selection.getStartOffset();
        final int endOffset = selection.getEndOffset();

	NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
		public Object run() {
		    nativeHighlightSelection(CurrentPageImpl.this.getNativeBrowserControl(), 
					     startContainer, endContainer, 
					     startOffset, endOffset);
		    return null;
		}
                public String toString() {
                    return "WCRunnable.nativeHighlightSelection";
                }

	    });
        }
    }

public void clearAllSelections() {
    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeClearAllSelections(CurrentPageImpl.this.getNativeBrowserControl());
		return null;
            }
            public String toString() {
                return "WCRunnable.nativeClearAllSelections";
            }

	});
    }
public void findInPage(String stringToFind, boolean forward, boolean matchCase)
{
    find(stringToFind, forward, matchCase);
}

public boolean find(String toFind, boolean dir, boolean doCase)
{
    final String stringToFind = toFind;
    final boolean forward = dir;
    final boolean matchCase = doCase;
    ParameterCheck.nonNull(stringToFind);
    Boolean result = Boolean.FALSE;

    result = (Boolean)
	NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable(){
	    public Object run() {
		boolean rc = nativeFind(CurrentPageImpl.this.getNativeBrowserControl(), 
					stringToFind, forward, matchCase);
		return rc ? Boolean.TRUE : Boolean.FALSE;
            }
            public String toString() {
                return "WCRunnable.nativeFind";
            }

	});
    return result.booleanValue();
}

public void findNextInPage()
{
    findNext();
}

public boolean findNext()
{
    Boolean result = Boolean.FALSE;
    
    result = (Boolean) 
	NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable(){
	    public Object run() {
		boolean rc = nativeFindNext(CurrentPageImpl.this.getNativeBrowserControl());
		return rc ? Boolean.TRUE : Boolean.FALSE;
	    }
            public String toString() {
                return "WCRunnable.nativeFindNext";
            }

	});
    return result.booleanValue();
}


public String getCurrentURL()
{
    String result = null;

    synchronized(getBrowserControl()) {
        result = nativeGetCurrentURL(getNativeBrowserControl());
    }
    return result;
}

public Document getDOM()
{
    // PENDING(edburns): run this on the event thread.
    Document result = nativeGetDOM(getNativeBrowserControl());
    if (LOGGER.isLoggable((Level.INFO))) {
        LOGGER.info("CurrentPageImpl.getDOM(): getting DOM with URI: " + 
                result.getDocumentURI());
    }
    return result;
}

public Properties getPageInfo()
{
  Properties result = null;

  /* synchronized(getBrowserControl()) {
        result = nativeGetPageInfo(getNativeBrowserControl());
    }
    return result;
    */

  throw new UnimplementedException("\nUnimplementedException -----\n API Function CurrentPage::getPageInfo has not yet been implemented.\n");
}



public String getSource()
{
    getWrapperFactory().verifyInitialized();
    Document doc = getDOM();
    String HTMLContent = null;
    final Selection selection = new SelectionImpl();

    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeGetSource(CurrentPageImpl.this.getNativeBrowserControl(),
				selection);
		return null;
	    }
            public String toString() {
                return "WCRunnable.nativeGetSource";
            }

	});
    HTMLContent = selection.toString();
    
    return HTMLContent;
}

public byte [] getSourceBytes()
{
    getWrapperFactory().verifyInitialized();
    byte [] result = null;

    String HTMLContent = getSource();
    if (null != HTMLContent) {
	result = HTMLContent.getBytes();
    }
    return result;
}

public void resetFind()
{
    getWrapperFactory().verifyInitialized();

    synchronized(getBrowserControl()) {
        nativeResetFind(getNativeBrowserControl());
    }
}

public void selectAll() {
    getWrapperFactory().verifyInitialized();
    
    NativeEventThread.instance.pushBlockingWCRunnable(new WCRunnable() {
	    public Object run() {
		nativeSelectAll(CurrentPageImpl.this.getNativeBrowserControl());
		return null;
	    }
            public String toString() {
                return "WCRunnable.nativeSelectAll";
            }

	});
}

public void print()
{
    NativeEventThread.instance.pushRunnable(new Runnable() {
	    public void run() {
		nativePrint(CurrentPageImpl.this.getNativeBrowserControl());
	    }
            public String toString() {
                return "Runnable.nativePrint";
            }


	});
}

public void printPreview(boolean pre)
{
    final boolean preview = pre;
    NativeEventThread.instance.pushRunnable(new Runnable() {
	    public void run() {
		nativePrintPreview(CurrentPageImpl.this.getNativeBrowserControl(), 
				   preview);
	    }
            public String toString() {
                return "Runnable.nativePrintPreview";
            }


	});
}

private StringSelection plainTextSelection = null;
private StringSelection htmlSelection = null;

void addStringToTransferable(String mimeType, String text) {
    if (null == mimeType || null == text) {
        if (LOGGER.isLoggable(Level.WARNING)) {
            LOGGER.warning("Browser requested addition of transferable with " + 
                    "null mimeType or text.");
        }
        return;
    }
    if (mimeType.equals("text/html")) {
        htmlSelection = new StringSelection(text);
    }
    if (mimeType.equals("text/unicode") || mimeType.equals("text/plain")) {
        plainTextSelection = new StringSelection(text);
    }
}

public void lostOwnership(Clipboard clipboard, Transferable contents) {
}


//
// Native methods
//

native public void nativeCopyCurrentSelectionToSystemClipboard(int webShellPtr);
native public void nativeGetSelection(int webShellPtr,
                                      Selection selection);

native public void nativeHighlightSelection(int webShellPtr, Node startContainer, Node endContainer, int startOffset, int endOffset);

native public void nativeClearAllSelections(int webShellPtr);

native public boolean nativeFind(int webShellPtr, String stringToFind, boolean forward, boolean matchCase);

native public boolean nativeFindNext(int webShellPtr);

native public String nativeGetCurrentURL(int webShellPtr);

native public Document nativeGetDOM(int webShellPtr);

// webclient.PageInfo getPageInfo();

native public void nativeGetSource(int webShellPtr, Selection selection);

native public void nativeResetFind(int webShellPtr);

native public void nativeSelectAll(int webShellPtr);

native public void nativePrint(int webShellPtr);

native public void nativePrintPreview(int webShellPtr, boolean preview);


} // end of class CurrentPageImpl
