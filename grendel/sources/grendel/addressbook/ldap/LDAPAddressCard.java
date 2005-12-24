/*
 * LDAPAddressCard.java
 *
 * Created on 23 September 2005, 22:11
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.ldap;

import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressBookFeildValueList;
import grendel.addressbook.AddressCard;
import java.io.IOException;
import java.util.Hashtable;
import java.util.Map;
import java.util.Set;
import netscape.ldap.LDAPAttribute;
import netscape.ldap.LDAPAttributeSet;
import netscape.ldap.LDAPEntry;
import netscape.ldap.LDAPException;
import netscape.ldap.LDAPModification;
import netscape.ldap.LDAPModificationSet;

/**
 *
 * @author hash9
 */
public class LDAPAddressCard extends AddressCard
{
  private LDAPEntry entry;
  private LDAPAddressBook book;
  
  /**
   * Creates a new instance of LDAPAddressCard
   */
  LDAPAddressCard(LDAPEntry entry, LDAPAddressBook book)
  {
    this.entry = entry;
    this.book = book;
  }
  
  public String toString() {
    return entry.getDN();
  }
  
  public String toLongString()
  {
    LDAPAttributeSet set = entry.getAttributeSet();
    StringBuilder sb = new StringBuilder("LDAPAddressCard:\n");
    for( int i = 0; i < set.size(); i++ )
    {
      if (i != 0)
      {
        sb.append("\n");
      }
      LDAPAttribute a = set.elementAt(i);
      sb.append("\t"); sb.append(a.getName()); sb.append("\n");
      {
        byte[][] b_a_a = a.getByteValueArray();
        if ( b_a_a.length > 0 )
        {
          for (int j = 0; j < b_a_a.length; j++)
          {
            if (j != 0)
            {
              sb.append("\n");
            }
            sb.append("\t\t");
            byte[] val = b_a_a[j];
            try
            {
              String sval = new String(val, "UTF8");
              if (sval.length() == 0 && val.length > 0)
              {
                sb.append("<binary value, length:");
                sb.append(val.length);
                sb.append(">");
              }
              else
              {
                sb.append(sval);
              }
              
            }
            catch (Exception e)
            {
              if (val != null)
              {
                sb.append("<binary value, length:");
                sb.append(val.length);
                sb.append(">");
              }
              else
              {
                sb.append("null value");
              }
            }
          }
        }
      }
    }
    return sb.toString();
  }
  
  public AddressBookFeildValueList getValues(String feild)
  {
    String[] strings = entry.getAttribute(feild).getStringValueArray();
    if (strings==null)
    {
      return null;
    }
    AddressBookFeildValueList values = new AddressBookFeildValueList(strings.length);
    for (int i =0;i<strings.length;i++)
    {
      values.add(new AddressBookFeildValue(strings[i]));
    }
    return values;
  }
  
  public Map<String, AddressBookFeildValueList> getAllValues()
  {
    LDAPAttributeSet atset = entry.getAttributeSet();
    Hashtable<String, AddressBookFeildValueList> map = new Hashtable<String, AddressBookFeildValueList>(atset.size());
    
    for (int i = 0;i<atset.size();i++)
    {
      LDAPAttribute at = atset.elementAt(i);
      map.put(at.getName(),AddressBookFeildValueList.fromStringArray(at.getStringValueArray()));
    }
    return map;
  }
  
  public void addValue(String feild, AddressBookFeildValue value) throws IOException
  {
    LDAPModificationSet mods = new LDAPModificationSet();
    
    LDAPAttribute attr = new LDAPAttribute(feild,value.getValue());
    
    mods.add(LDAPModification.ADD, attr);
    try
    {
      book.connection.modify(entry.getDN(),mods);
    }
    catch (LDAPException le)
    {
      throw LDAPAddressBook.makeIOE(le);
    }
    update();
  }
  
  public void addMultiValue(Map<String,AddressBookFeildValue> values) throws IOException
  {
    LDAPModificationSet mods = new LDAPModificationSet();
    Set<String> keys = values.keySet();
    for (String key: keys)
    {
      AddressBookFeildValue value = values.get(key);
      LDAPAttribute attr = new LDAPAttribute(key,value.getValue());
      mods.add(LDAPModification.ADD, attr);
    }
    try
    {
      book.connection.modify(entry.getDN(),mods);
    }
    catch (LDAPException le)
    {
      throw LDAPAddressBook.makeIOE(le);
    }
    update();
  }
  
  private void update() throws IOException
  {
    entry = book.getAddressCardRef(entry.getDN());
  }
  
}
