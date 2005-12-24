/*
 * AddressList.java
 *
 * Created on 23 September 2005, 22:13
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc;

import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressBookFeildValueList;
import grendel.addressbook.AddressCard;
import grendel.addressbook.AddressCardList;

import grendel.storage.addressparser.AddressList;

import java.sql.ResultSet;
import java.sql.SQLException;

import java.util.Hashtable;
import java.util.Set;


/**
 *
 * @author hash9
 */
public class JDBCAddressCardList extends AddressCardList
{
  /**
   * Creates a new instance of JDBCAddressCardList from a given result set.
   * This assumes that the result set is for the standard db schema.
   */
  public JDBCAddressCardList()
  {
  }

  public JDBCAddressCardList(ResultSet results) throws SQLException
  {
    /*if (results.igetRow() == 0) //XXX Broken (Maybe not neaded)
    {
      return; //There are no rows it's an empty list we have nothing to do.
    }*/
    Hashtable<Integer,Object> records = new Hashtable<Integer,Object>(100);

    while (results.next())
    {
      try
      {
        Integer id = results.getInt("id");
        String feild = results.getString("feild");
        String value = results.getString("value");
        String flags = results.getString("flags");
        Hashtable<String,AddressBookFeildValueList> feilds = (Hashtable<String,AddressBookFeildValueList>) records.get(id);

        if (feilds == null)
        {
          feilds = new Hashtable<String,AddressBookFeildValueList>();
          records.put(id, feilds);
        }

        AddressBookFeildValueList values = feilds.get(feild);

        if (values == null)
        {
          values = new AddressBookFeildValueList();
          feilds.put(feild, values);
        }
        
        AddressBookFeildValue ab_value = new AddressBookFeildValue(value);
        flags = flags.toLowerCase();
        ab_value.setReadonly(flags.contains("r"));
        ab_value.setHidden(flags.contains("h"));
        ab_value.setSystem(flags.contains("s"));
        values.add(ab_value);
      }
      catch (SQLException ex)
      {
        ex.printStackTrace(); //XXX Make this print all the nested exceptions and publish them.
      }
    }

    Set<Integer> keys = records.keySet();

    for (Integer id : keys)
    {
      Hashtable<String,AddressBookFeildValueList> feilds = (Hashtable<String,AddressBookFeildValueList>) records.get(id);
      this.add(new JDBCAddressCard(id, feilds));
    }

    //
    //Hashtable<String, AddressBookFeildValue[]> feilds
    records = null; //Release the memmory for garbage collection
    System.gc(); //XXX We've posibly used a lot of memory get rid of it.
  }

  public AddressList getAddressList()
  {
    AddressList al = new AddressList();

    for (AddressCard card : this)
    {
      ;
    }

    return al;
  }

  public String toLongString()
  {
    StringBuilder sb = new StringBuilder("JDBCAddressCardList:\n");

    for (AddressCard card : this)
    {
      sb.append("\t");
      sb.append(card.toString().replace("\n", "\n\t"));
      sb.append("\n");
    }

    return sb.toString();
  }

  public String toString()
  {
    String s = super.toString();

    return s;
  }
}
