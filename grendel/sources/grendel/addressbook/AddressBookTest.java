/*
 * AddressBookTest.java
 *
 * Created on 06 October 2005, 15:30
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;

import grendel.addressbook.query.QueryStatement;

import java.io.IOException;


/**
 * Test various elements of the Address book archetecture.
 * @author Kieran Maclean
 */
public class AddressBookTest
{
  /** Creates a new instance of AddressBookTest */
  public AddressBookTest()
  {
  }

  /**
   * @param args the command line arguments
   */
  public static void main(String[] args) throws IOException
  {
    /*Addressbook_LDAP addressbook = new Addressbook_LDAP();
    addressbook.setName("St Andrews LDAP");
    addressbook.setPort(389);
    addressbook.setHost("ldap.st-and.ac.uk");
    addressbook.setSearchBase("dc=st-andrews,dc=ac,dc=uk");
    AddressBookManager.addAddressbook(addressbook);
    Preferences.save();*/
    AddressBook book = AddressBookManager.getAddressBook("St Andrews LDAP");
    book.connect();

    //LDAPSearchResults results =  book.getAddressCardList("(objectclass=*)");
    AddressCardList results = book.getAddressCardList(new QueryStatement().or(
          new QueryStatement().feildContains("uid", "kam58")
                              .feildContains("cn", "Kieran"),
          new QueryStatement().feildContains("uid", "hjep")
                              .feildContains("cn", "Holly")));

    //AddressCardList results =  book.getAddressCardList("|(uid = *kam58*)(uid = *hjep*))");
    //AddressCardList results = book.getAddressCardList(new QueryStatement().or(new QueryStatement().feildContains("uid","kam58"),new QueryStatement().feildContains("uid","hjep")));
    System.out.println(results.toString());

    /*for (AddressCard card: results) {
      System.out.println(card.toString());
    }*/
  }
}
