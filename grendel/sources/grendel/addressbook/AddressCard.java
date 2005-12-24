/*
 * AddressCard.java
 *
 * Created on 23 September 2005, 22:11
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;

import java.io.IOException;
import java.util.List;

import java.util.Map;


/**
 * An Address Card. This contains all the data for a given address card.
 * This specification defines a mutible address card.
 * @TODO define an immutible address card
 * @author Kieran Maclean
 */
public abstract class AddressCard
{
  protected AddressCard()
  {
  }

  /**
   * Add the given values to the address card.
   */
  public abstract void addMultiValue(Map<String,AddressBookFeildValue> values)
    throws IOException;

  /**
   * Add the given value to the address card.
   */
  public abstract void addValue(String feild, AddressBookFeildValue value)
    throws IOException;

  /**
   * @return a Map of all the values in this Address card.
   */
  public abstract Map<String,AddressBookFeildValueList> getAllValues();

  /**
   *
   * @return an array of Values for the given feild.
   */
  public abstract AddressBookFeildValueList getValues(String feild);

  /**
   * Accessor method is <code>getValue("cn").getValue();</code>
   * @return the name of the person the address card refers to.
   */
  public String getName()
  {
    return getValue("cn").getValue();
  }

   public String getEMail() {
        return  getValue("mail").getValue();
    }
  
  /**
   * @return the first value for a given feild or null if the feild has no values or is the empty set of values.
   */
  public AddressBookFeildValue getValue(String feild)
  {
    AddressBookFeildValueList values = getValues(feild);

    if ((values != null) && (values.size() > 0))
    {
      return values.get(0);
    }

    return null;
  }

  /**
   *
   * @TODO Define what is meant by this method or remove it from the API
   */
  public abstract String toLongString();

  /**
   * @return a string to discribe this address card.
   */
  public abstract String toString();

   
}
