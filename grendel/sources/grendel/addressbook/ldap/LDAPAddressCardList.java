/*
 * AddressList.java
 *
 * Created on 23 September 2005, 22:13
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.ldap;

import grendel.addressbook.AddressCard;
import grendel.addressbook.AddressCardList;
import grendel.messaging.NoticeBoard;
import grendel.messaging.StringNotice;
import grendel.storage.addressparser.AddressList;
import netscape.ldap.LDAPException;
import netscape.ldap.LDAPSearchResults;

/**
 *
 * @author hash9
 */
public class LDAPAddressCardList extends AddressCardList
{
  
  /**
   * Creates a new instance of LDAPAddressCardList
   */
  LDAPAddressCardList(LDAPSearchResults results, LDAPAddressBook book)
  {
    while(results.hasMoreElements())
    {
      try
      {
        this.add(new LDAPAddressCard(results.next(),book));
      }
      catch (LDAPException ex)
      {
        String message = ex.getLDAPErrorMessage();
        if ((message==null)||message.length()==0) {
          message = "LDAP: " + ex.errorCodeToString();
        }
        NoticeBoard.publish(new StringNotice(message));
      }
    }
  }
  
  public AddressList getAddressList()
  {
    AddressList al = new AddressList();
    for (AddressCard card: this)
    {
      
    }
    return al;
  }
  
  public String toLongString()
  {
    StringBuilder sb = new StringBuilder("LDAPAddressCardList:\n");
    for (AddressCard card: this)
    {
      sb.append("\t");
      sb.append(card.toLongString().replace("\n","\n\t"));
      sb.append("\n");
    }
    return sb.toString();
  }
}
