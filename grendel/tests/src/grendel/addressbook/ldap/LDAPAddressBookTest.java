/*
 * LDAPAddressBookTest.java
 * JUnit based test
 *
 * Created on 24 September 2005, 13:11
 */

package grendel.addressbook.ldap;

import grendel.addressbook.AddressCardList;
import grendel.messaging.NoticeBoard;
import java.io.IOException;
import junit.framework.*;
import grendel.prefs.addressbook.Addressbook_LDAP;

/**
 *
 * @author hash9
 */
public class LDAPAddressBookTest extends TestCase
{
  
  public LDAPAddressBookTest(String testName)
  {
    super(testName);
  }

  protected void setUp() throws Exception
  {
  }

  protected void tearDown() throws Exception
  {
  }

  public static Test suite()
  {
    TestSuite suite = new TestSuite(LDAPAddressBookTest.class);
    
    return suite;
  }

  /**
   * Test the class
   */
  public void testLDAPClass()
  {
    NoticeBoard.getNoticeBoard().removePublisher(NoticeBoard.getNoticeBoard().listPublishers().get(1));
    Addressbook_LDAP addressbook = new Addressbook_LDAP();
    addressbook.setName("St Andrews LDAP");
    addressbook.setPort(389);
    addressbook.setHost("ldap.st-and.ac.uk");
    addressbook.setSearchBase("dc=st-andrews,dc=ac,dc=uk");
    LDAPAddressBook book = new LDAPAddressBook(addressbook);
    try
    {
      book.connect();
      AddressCardList results = book.getAddressCardList((String) null);
      System.out.println(results.toString());
    } catch (IOException ex)
    {
      ex.printStackTrace();
       fail(ex.getMessage());
    }
  }
}
