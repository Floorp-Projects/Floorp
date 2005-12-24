/*
 * AddressBook.java
 *
 * Created on 23 September 2005, 22:19
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.ldap;
import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressBookFeildValueList;
import grendel.addressbook.AddressCard;
import grendel.addressbook.AddressCardList;
import grendel.addressbook.query.QueryStatement;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.prefs.addressbook.Addressbook_LDAP;
import java.io.IOException;
import java.util.Map;
import java.util.Set;
import netscape.ldap.LDAPAttribute;
import netscape.ldap.LDAPAttributeSet;
import netscape.ldap.LDAPConnection;
import netscape.ldap.LDAPEntry;
import netscape.ldap.LDAPException;
import netscape.ldap.LDAPv3;
/**
 *
 * @author hash9
 */
public class LDAPAddressBook extends grendel.addressbook.AddressBook
{
  LDAPConnection connection;
  private Addressbook_LDAP addressbook;
  
  /**
   * Creates a new instance of LDAPAddressBook
   */
  public LDAPAddressBook(Addressbook_LDAP addressbook)
  {
    this.addressbook = addressbook;
  }
  
  static IOException makeIOE(LDAPException le)
  {
    IOException ioe = new IOException("LDAP Exception: "+le.errorCodeToString());
    ioe.initCause(le);
    return ioe;
  }
  
  public void connect() throws IOException
  {
    if (! isConnected())
    {
      connection = new LDAPConnection();
      try
      {
        connection.setOption(LDAPConnection.SIZELIMIT,100);
        if ((addressbook.getDN()==null)||(addressbook.getDN().equals("")))
        { // Don't logon
          connection.connect(addressbook.getHost(),addressbook.getPort());
        }
        else
        { // Logon
          connection.connect(addressbook.getHost(),addressbook.getPort(),addressbook.getDN(),addressbook.getPassword());
        }
      }
      catch (LDAPException le)
      {
        //XXX The LDAPException should be parsed better than it is.
        NoticeBoard.publish(new ExceptionNotice(le));
        throw makeIOE(le);
      }
    }
  }
  
  public boolean isConnected()
  {
    if (connection!=null)
    {
      return connection.isConnected();
    }
    return false;
  }
  
  LDAPEntry getAddressCardRef(String ref) throws IOException
  {
    try
    {
      return connection.read(ref);
    }
    catch (LDAPException le)
    {
      throw makeIOE(le);
    }
  }
  
  public AddressCard getAddressCard(String ref) throws IOException
  {
    try
    {
      return new LDAPAddressCard(connection.read(ref),this);
    }
    catch (LDAPException le)
    {
      throw makeIOE(le);
    }
  }
  
  public AddressCardList getAddressCardList(QueryStatement qs) throws IOException
  {
    return getAddressCardList(qs.makeLDAP());
  }
  
  public AddressCardList getAddressCardList(String filter) throws IOException
  {
    try
    {
      return new LDAPAddressCardList(connection.search(addressbook.getSearchBase(), LDAPv3.SCOPE_SUB,filter,null,false),this);
    }
    catch (LDAPException le)
    {
      throw makeIOE(le);
    }
  }
  
  public void addNewCard(AddressCard ac) throws IOException
  {
    AddressBookFeildValue base = ac.getValue("base");
    if (!(base.isSystem()&&base.isReadonly()&&base.isHidden())) throw new IllegalArgumentException("card must have a base for LDAP insert"); //TODO try to remove this requirement
    String dn = "cn="+ac.getName()+ ","+ base.getValue() + ","+ addressbook.getSearchBase();
    
    Map<String,AddressBookFeildValueList> map = ac.getAllValues();
    
    LDAPAttributeSet attrs = new LDAPAttributeSet();
    
    
    Set<String> keys = map.keySet();
    for(String key:keys)
    {
      LDAPAttribute attr = new LDAPAttribute(key, AddressBookFeildValueList.toStringArray(map.get(key)) );
      attrs.add( attr );
    }
    
    LDAPEntry newEntry = new LDAPEntry( dn, attrs );
    
    try
    {
      connection.add( newEntry );
    }
    catch (LDAPException le)
    {
      throw LDAPAddressBook.makeIOE(le);
    }
  }
  
  public String toString()
  {
    return addressbook.getName();
  }
}
