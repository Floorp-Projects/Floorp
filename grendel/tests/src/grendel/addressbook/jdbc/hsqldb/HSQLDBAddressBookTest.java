/*
 * HSQLDBAddressBookTest.java
 * JUnit based test
 *
 * Created on 17 October 2005, 19:07
 */

package grendel.addressbook.jdbc.hsqldb;

import grendel.addressbook.AddressCardList;
import grendel.addressbook.jdbc.JDBCRegister;
import grendel.addressbook.jdbc.derby.DerbyAddressBook;
import grendel.prefs.addressbook.Addressbook_HSQLDB;
import junit.framework.*;
import grendel.messaging.NoticeBoard;
import grendel.prefs.addressbook.Addressbook_Derby;
import grendel.Shutdown;

/**
 *
 * @author hash9
 */
public class HSQLDBAddressBookTest extends TestCase
{
  
  public HSQLDBAddressBookTest(String testName)
  {
    super(testName);
  }

    protected void setUp() throws Exception {
    }

    protected void tearDown() throws Exception {
    }

    public static Test suite() {
        TestSuite suite = new TestSuite(HSQLDBAddressBookTest.class);
        
        return suite;
    }

    /**
     * Test of connect method, of class grendel.addressbook.jdbc.hsqldb.HSQLDBAddressBook.
     */
public void testDerbyClass() throws Exception
  {
    JDBCRegister.registerJDBCdrivers();
    NoticeBoard.getNoticeBoard().clearPublishers();
    Shutdown.touch();
    Addressbook_HSQLDB book = new Addressbook_HSQLDB();
    book.setName("HSQLDB Test Database");
    HSQLDBAddressBook ab = new HSQLDBAddressBook(book);
    ab.connect();
    AddressCardList cards = ab.getAddressCardList((String)null);
    System.out.println(cards.toString());
  }
  
}
