/*
 * AddressBookFeildValue.java
 *
 * Created on 30 September 2005, 14:48
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;

import java.util.ArrayList;
import java.util.List;
import java.util.Vector;


/**
 * A list of values for a address card feild.
 * This is a list of values value for a single feild in an address book.
 * @author Kieran Maclean
 */
public class AddressBookFeildValueList extends Vector<AddressBookFeildValue>
  implements List<AddressBookFeildValue>
{
  public AddressBookFeildValueList()
  {
    super();
  }

  public AddressBookFeildValueList(int size)
  {
    super(size);
  }

  /**
   * @return a AddressBookFeildValue array of the given String array. This is a convince method.
   */
  public static AddressBookFeildValueList fromStringArray(String[] s_a)
  {
    AddressBookFeildValueList values = new AddressBookFeildValueList();

    for (int i = 0; i < s_a.length; i++)
    {
      values.add(new AddressBookFeildValue(s_a[i]));
    }

    return values;
  }

  /**
   *
   * @return a String array of the given values. This is a convince method.
   */
  public static String[] toStringArray(AddressBookFeildValueList values)
  {
    String[] s_a = new String[values.size()];

    for (int i = 0; i < values.size(); i++)
    {
      s_a[i] = values.get(i).getValue();
    }

    return s_a;
  }
}
