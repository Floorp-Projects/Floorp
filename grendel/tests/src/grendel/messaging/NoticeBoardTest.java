/*
 * NoticeBoardTest.java
 * JUnit based test
 *
 * Created on 22 August 2005, 22:34
 */

package grendel.messaging;

import junit.framework.*;


/**
 *
 * @author hash9
 */
public class NoticeBoardTest extends TestCase {
    
    public NoticeBoardTest(String testName) {
        super(testName);
    }
    
    protected void setUp() throws Exception {
    }
    
    protected void tearDown() throws Exception {
    }
    
    public static Test suite() {
        TestSuite suite = new TestSuite(NoticeBoardTest.class);
        
        return suite;
    }
    
    private Publisher pub;
    public static class TestNotice extends Notice {
        String message;
        public TestNotice(String message) {
            assertNotNull(message);
            assertTrue(! message.toString().equals(""));
            this.message = message;
        }
        public String toString() {
            assertNotNull(message);
            assertTrue(! message.toString().equals(""));
            return message;
        }
    }
    private static boolean published = false;
    private static class TestPublisher implements Publisher {
        public void publish(Notice notice) {
            assertNotNull(notice);
            assertTrue(! notice.toString().equals(""));
            published = true;
        }
    }
        
    /**
     * Test of publish method, of class grendel.messaging.NoticeBoard.
     */
    public void testPublish() {
        System.out.println("addPublisher");
        pub = new TestPublisher();
        NoticeBoard.getNoticeBoard().addPublisher(pub);
        System.out.println("publish");
        String s = "Test";
        Notice notice = new TestNotice(s);
        NoticeBoard.publish(notice);
        synchronized(this) {
            try {
                wait(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        assertTrue(published);
        published = false;
        System.out.println("removePublisher");
        NoticeBoard.getNoticeBoard().removePublisher(pub);
    }
    
}
