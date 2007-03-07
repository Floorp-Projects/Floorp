/*
 * CarDemoTest.java
 * JUnit based test
 *
 * Created on March 3, 2007, 2:31 PM
 */

package cardemo;

import org.mozilla.mcp.MCP;
import org.mozilla.webclient.WebclientTestCase;
import org.w3c.dom.Element;

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
    
    public void testCardemo() throws Exception {
        mcp.getRealizedVisibleBrowserWindow();
        
        // Load the main page of the app
        mcp.blockingLoad("http://webdev1.sun.com/jsf-ajax-cardemo/faces/chooseLocale.jsp");
        // Choose the "German" language button
        mcp.blockingClickElement("j_id_id73:Germany");
        // Choose the roadster
        mcp.blockingClickElement("j_id_id18:j_id_id43");
        // Choose the "Tempomat" checkbox
        mcp.clickElement("j_id_id21:j_id_id67j_id_1");

        Thread.currentThread().sleep(10000);
    }
    
}
