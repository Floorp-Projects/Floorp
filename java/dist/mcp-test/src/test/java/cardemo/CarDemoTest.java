/*
 * CarDemoTest.java
 * JUnit based test
 *
 * Created on March 3, 2007, 2:31 PM
 */

package cardemo;

import java.awt.Robot;
import java.awt.event.InputEvent;
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
        mcp.blockingLoad("http://webdev1.sun.com/jsf-ajax-cardemo/faces/chooseLocale.jsp");
        Element germanButton = mcp.getElementInCurrentPageById("j_id_id73:Germany");
        assertNotNull(germanButton);
        String clientX = germanButton.getAttribute("clientX");
        String clientY = germanButton.getAttribute("clientY");
        assertNotNull(clientX);
        assertNotNull(clientY);
        int x = Integer.valueOf(clientX).intValue();
        int y = Integer.valueOf(clientY).intValue();
	Robot robot = new Robot();
	
	robot.mouseMove(x, y);
	robot.mousePress(InputEvent.BUTTON1_MASK);
	robot.mouseRelease(InputEvent.BUTTON1_MASK);

        Thread.currentThread().sleep(30000);
    }
    
}
