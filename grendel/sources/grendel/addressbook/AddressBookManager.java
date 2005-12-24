/*
 * AddressbookManager.java
 *
 * Created on 23 September 2005, 22:10
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;

import grendel.addressbook.jdbc.derby.DerbyAddressBook;

import grendel.addressbook.ldap.LDAPAddressBook;

import grendel.prefs.Preferences;

import grendel.prefs.addressbook.Addressbook;
import grendel.prefs.addressbook.Addressbook_Derby;
import grendel.prefs.addressbook.Addressbook_LDAP;
import grendel.prefs.addressbook.Addressbooks;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Set;


/**
 * Manages the Addressbooks. Provides methods to add an address book and retrive an instance of an existing one.
 * @author hash9
 */
public final class AddressBookManager
{
  /**
   * The branch of the Preferances with the Addressbooks in it.
   */
  private static Addressbooks books;

  static
  {
    /*
     * Get the addressbooks branch from preferances.
     * Done here incase any other initation requires it.
     */
    books = Preferences.getPreferances().getAddressbooks();
  }

  /**
   * Prevent the non-instanciable Class from being instancated.
   */
  private AddressBookManager()
  {
  }

  /**
   * Add an addressbook to the stored addressbooks.
   */
  public static void addAddressbook(Addressbook book)
  {
    books.addAddressbook(book);
  }

  /**
   * Get an instance of a named address book.
   */
  public static AddressBook getAddressBook(String name)
  {
    Addressbook book = books.getAddressbook(name);

    return createAddressBookInstance(book);
  }

  /**
   * Get the names of all the address books.
   */
  public static Set<String> getAddressBookSet()
  {
    return books.keySet();
  }

  /**
   * Get an instance of all address books.
   */
  public static List<AddressBook> getAllAddressBooks()
  {
    List<AddressBook> l = new ArrayList<AddressBook>(books.size());
    Collection<Object> c = books.values();

    for (Object o : c)
    {
      l.add(createAddressBookInstance((Addressbook) o));
    }

    return l;
  }

  /**
   * Actually create the instances of the address books.
   * @TODO Cache Addressbook referances we shouldn't reinstaciate an address book. (Use a week map)
   */
  private static AddressBook createAddressBookInstance(Addressbook book)
    throws IllegalArgumentException
  {
    String s_type = book.getSuperType();
    String type = book.getType();

    String classname = AddressBook.class.getPackage().getName();

    if (!((s_type == null) || (s_type.equals(""))))
    {
      classname = classname.concat(".").concat(s_type);
    }

    classname = classname.concat(".").concat(type.toLowerCase()).concat(".");
    classname = classname.concat(type).concat("AddressBook");

    Constructor constr;
    
    try
    {
      constr = Class.forName(classname)
                    .getDeclaredConstructor(book.getClass());
    }
    catch (Exception ex) // If something goes wrong
    {
      throw new IllegalArgumentException(ex.getClass().getSimpleName() + ": " +
        ex.getMessage(), ex);
    }

    try
    {
      return (AddressBook) constr.newInstance(book);
    }
    catch (Exception ex) // If something goes wrong
    {
      throw new IllegalArgumentException(ex.getClass().getSimpleName() + ": " +
        ex.getMessage(), ex);
    }
  }
}
