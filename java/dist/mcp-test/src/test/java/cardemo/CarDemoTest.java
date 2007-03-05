/*
 * CarDemoTest.java
 * JUnit based test
 *
 * Created on March 3, 2007, 2:31 PM
 */

package cardemo;

import org.mozilla.mcp.MCP;
import org.mozilla.webclient.WebclientTestCase;

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
        mcp.blockingLoad("http://webdev1.sun.com/jsf-ajax-cardemo/faces/chooseLocale.jsp");
        Thread.currentThread().sleep(10000);
    }
    
}
