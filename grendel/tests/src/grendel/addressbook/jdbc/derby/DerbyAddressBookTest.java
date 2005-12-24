/*
 * DerbyAddressBookTest.java
 * JUnit based test
 *
 * Created on 16 October 2005, 23:09
 */

package grendel.addressbook.jdbc.derby;

import grendel.addressbook.jdbc.JDBCRegister;
import grendel.messaging.NoticeBoard;
import junit.framework.*;
import grendel.addressbook.AddressCardList;
import grendel.prefs.addressbook.Addressbook_Derby;
import grendel.Shutdown;

/**
 *
 * @author hash9
 */
public class DerbyAddressBookTest extends TestCase
{
  
  public DerbyAddressBookTest(String testName)
  {
    super(testName);
  }

  public static Test suite()
  {
    TestSuite suite = new TestSuite(DerbyAddressBookTest.class);
    
    return suite;
  }

  /**
   * Test of connect method, of class grendel.addressbook.jdbc.derby.DerbyAddressBook.
   */
  public void testDerbyClass() throws Exception
  {
    JDBCRegister.registerJDBCdrivers();
    NoticeBoard.getNoticeBoard().clearPublishers();
    Shutdown.touch();
    Addressbook_Derby book = new Addressbook_Derby();
    book.setName("Derby Basic Test");
    DerbyAddressBook dab = new DerbyAddressBook(book);
    dab.connect();
    AddressCardList cards = dab.getAddressCardList((String)null);
    System.out.println(cards.toString());
  }
}
