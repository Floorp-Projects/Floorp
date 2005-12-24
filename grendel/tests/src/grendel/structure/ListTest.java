/*
 * ListTest.java
 * JUnit based test
 *
 * Created on 22 August 2005, 22:53
 */

package grendel.structure;

import junit.framework.*;

/**
 *
 * @author hash9
 */
public class ListTest extends TestCase {
    
    public ListTest(String testName) {
        super(testName);
    }
    
    protected void setUp() throws Exception {
    }
    
    protected void tearDown() throws Exception {
    }
    
    public static Test suite() {
        TestSuite suite = new TestSuite(ListTest.class);
        
        return suite;
    }
    
    public void runTest() {
        testList();
    }
    
    public void testList() {
        System.err.println("Servers: "+Servers.getServers());
        Server s = Servers.getServers().get(2);
        StringBuilder sb = new StringBuilder();
        decend(s.getRoot(),"",sb);
        System.out.println(sb.toString());
    }
    
    public void decend(Folder f, String indent, StringBuilder sb) {
        sb.append(indent + "Folder: "+f.getName()+"\n");
        sb.append(indent + "\tMessages:"+"\n");
        MessageList ml = f.getMessages();
        assertNotNull(ml);
        int i = 0;
        for (Message m: ml) {
            sb.append(indent + "\t\t"+m.toString()+"\n");
            i++;
            if (i>10) break;
        }
        FolderList fl = f.getFolders();
        assertNotNull(fl);
        indent = indent+"\t";
        i = 0;
        for (Folder nf: fl) {
            decend(nf,indent,sb);
            i++;
            if (i>10) break;
        }
    }
    
}
