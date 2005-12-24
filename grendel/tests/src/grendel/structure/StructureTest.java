/*
 * StructureTest.java
 * JUnit based test
 *
 * Created on 22 August 2005, 22:24
 */

package grendel.structure;

import grendel.javamail.JMProviders;
import java.util.List;
import javax.mail.Session;
import junit.framework.*;

/**
 *
 * @author hash9
 */
public class StructureTest extends TestCase {
    
    public StructureTest(String testName) {
        super(testName);
    }
    
    protected void setUp() throws Exception {
    }
    
    protected void tearDown() throws Exception {
    }
    
    public static Test suite() {
        TestSuite suite = new TestSuite(StructureTest.class);
        
        return suite;
    }
    
    
    /**
     * Test of getServers method, of class grendel.structure.Servers.
     */
    public void testGetServers() {
        System.out.println("getServers");
        
        List<Server> result = Servers.getServers();
        assertNotNull(result);
    }
    
    /**
     * Test of getSession method, of class grendel.structure.Servers.
     */
    public void testGetSession() {
        System.out.println("getSession");
        
        Session result = JMProviders.getSession();
        assertNotNull("Session",result);
        
    }
    
}
