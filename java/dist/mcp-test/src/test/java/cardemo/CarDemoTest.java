/*
 * $Id: CarDemoTest.java,v 1.7 2007/03/15 00:33:10 edburns%acm.org Exp $
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
package cardemo;

import java.util.BitSet;
import java.util.Map;
import junit.framework.TestFailure;
import org.mozilla.mcp.AjaxListener;
import org.mozilla.mcp.MCP;
import org.mozilla.webclient.WebclientTestCase;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.Document;

/**
 *
 * @author edburns
 */
public class CarDemoTest extends WebclientTestCase  {
    
    private MCP mcp = null;    
    
    public CarDemoTest(String testName) {
        super(testName);
    }

    public void setUp() {
        super.setUp();

        mcp = new MCP();
	try {
	    mcp.setAppData(getBrowserBinDir());
	}
	catch (Exception e) {
	    fail();
	}
        
    }
    
    enum TestFeature {
        RECEIVED_END_AJAX_EVENT,
        HAS_MAP,
        HAS_VALID_RESPONSE_TEXT,
        HAS_VALID_RESPONSE_XML,
        HAS_VALID_READYSTATE,
        STOP_WAITING
    }
    
    public void testCardemo() throws Exception {
        mcp.getRealizedVisibleBrowserWindow();
        final BitSet bitSet = new BitSet();
        AjaxListener listener = new AjaxListener() {
            public void endAjax(Map eventMap) {
                bitSet.flip(TestFeature.RECEIVED_END_AJAX_EVENT.ordinal());
                if (null != eventMap) {
                    bitSet.flip(TestFeature.HAS_MAP.ordinal());
                }
		// Make some assertions about the response text
                String responseText = (String) eventMap.get("responseText");
                if (null != responseText) {
                    if (-1 != responseText.indexOf("<partial-response>") &&
                        -1 != responseText.indexOf("</partial-response>")) {
                        bitSet.flip(TestFeature.HAS_VALID_RESPONSE_TEXT.ordinal());
                    }
                }
		Document responseXML = (Document) 
		    eventMap.get("responseXML");
                Element rootElement = null, element = null;
                Node node = null;
                String tagName = null;
                try {
                    rootElement = responseXML.getDocumentElement();
                    tagName = rootElement.getTagName();
                    if (tagName.equals("partial-response")) {
                        element = (Element) rootElement.getFirstChild();
                        tagName = element.getTagName();
                        if (tagName.equals("components")) {
                            element = (Element) rootElement.getLastChild();
                            tagName = element.getTagName();
                            if (tagName.equals("state")) {
                                bitSet.flip(TestFeature.
                                        HAS_VALID_RESPONSE_XML.ordinal());
                            }
                        }
                    }
                }
                catch (Throwable t) {
                    
                }
		
                String readyState = (String) eventMap.get("readyState");
                bitSet.set(TestFeature.HAS_VALID_READYSTATE.ordinal(), 
                        null != readyState && readyState.equals("4"));
                bitSet.flip(TestFeature.STOP_WAITING.ordinal());
                
            }
        };
        mcp.addAjaxListener(listener);
        
        // Load the main page of the app
        mcp.blockingLoad("http://javaserver.org/jsf-ajax-cardemo/faces/chooseLocale.jsp");
        // Choose the "German" language button
        mcp.blockingClickElement("j_id_id73:Germany");
        // Choose the roadster
        mcp.blockingClickElement("j_id_id18:j_id_id43");
        // Sample the Basis-Preis and Ihr Preis before the ajax transaction
        Element pricePanel = mcp.findElement("j_id_id10:zone1");
        assertNotNull(pricePanel);
        String pricePanelText = pricePanel.getTextContent();
        
        assertNotNull(pricePanelText);
        assertTrue(pricePanelText.matches("(?s).*Basis-Preis\\s*15700.*"));
        assertTrue(pricePanelText.matches("(?s).*Ihr Preis\\s*15700.*"));
        
        // Choose the "Tempomat" checkbox
        bitSet.clear();
        mcp.clickElement("j_id_id10:j_id_id63j_id_1");
        
        while (!bitSet.get(TestFeature.STOP_WAITING.ordinal())) {
            Thread.currentThread().sleep(5000);
        }
        
        // assert that the ajax transaction succeeded
        assertTrue(bitSet.get(TestFeature.RECEIVED_END_AJAX_EVENT.ordinal()));
        assertTrue(bitSet.get(TestFeature.HAS_MAP.ordinal()));
        assertTrue(bitSet.get(TestFeature.HAS_VALID_RESPONSE_TEXT.ordinal()));
        assertTrue(bitSet.get(TestFeature.HAS_VALID_RESPONSE_XML.ordinal()));
        assertTrue(bitSet.get(TestFeature.HAS_VALID_READYSTATE.ordinal()));
        bitSet.clear();
        
        // Sample the Basis-Preis and Ihr-Preis after the ajax transaction
        pricePanel = mcp.findElement("j_id_id10:zone1");
        assertNotNull(pricePanel);
        pricePanelText = pricePanel.getTextContent();

        assertNotNull(pricePanelText);
        assertTrue(pricePanelText.matches("(?s).*Basis-Preis\\s*15700.*"));
        assertTrue(pricePanelText.matches("(?s).*Ihr Preis\\s*16600.*"));

        mcp.deleteBrowserControl();
    }
    
}
