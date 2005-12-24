/*
 * JDBCAddressCard.java
 *
 * Created on 23 September 2005, 22:11
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc;

import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressBookFeildValueList;
import grendel.addressbook.AddressCard;

import java.io.IOException;

import java.sql.SQLException;

import java.util.Hashtable;
import java.util.Map;
import java.util.Set;


/**
 *
 * @author hash9
 */
public class JDBCAddressCard extends AddressCard
{
  protected Hashtable<String,AddressBookFeildValueList> feilds = new Hashtable<String,AddressBookFeildValueList>();
  private JDBCAddressBook addressbook;
  private int id;

  /**
   * Creates a new instance of JDBCAddressCard
   */
  public JDBCAddressCard(int id,
    Hashtable<String,AddressBookFeildValueList> feilds)
  {
    this.id = id;
    this.feilds = feilds;
  }

  protected JDBCAddressCard(JDBCAddressBook addressbook)
  {
    this.addressbook = addressbook;
  }

  public void addMultiValue(Map<String,AddressBookFeildValue> values)
    throws IOException
  {
    try
    {
      Set<String> keys = values.keySet();

      for (String key : keys)
      {
        AddressBookFeildValue val = values.get(key);
        addressbook.addValue(id, key, val);
      }
    }
    catch (SQLException ex)
    {
      throw new IOException(ex.getMessage());
    }
  }

  public void addValue(String feild, AddressBookFeildValue value)
    throws IOException
  {
    try
    {
      addressbook.addValue(id, feild, value);
    }
    catch (SQLException ex)
    {
      throw new IOException(ex.getMessage());
    }
  }

  public Map<String,AddressBookFeildValueList> getAllValues()
  {
    return feilds;
  }

  public AddressBookFeildValueList getValues(String feild)
  {
    return feilds.get(feild);
  }

  public String toLongString()
  {
    return toString(); //Define this method
  }

  public String toString()
  {
    return feilds.toString();
  }
}
