/*
 * Addressbook.java
 *
 * Created on 23 September 2005, 22:10
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;

import grendel.addressbook.query.QueryStatement;
import grendel.prefs.addressbook.Addressbook;

import java.io.IOException;


/**
 * An Address book instance.
 * @author Kieran Maclean
 */
public abstract class AddressBook
{
  protected AddressBook()
  {
  }

  /**
   * Add the given address card to the address book.
   * @throws java.io.IOException If something goes wrong.
   */
  public abstract void addNewCard(AddressCard ac) throws IOException;

  /**
   * Connect to the address book. This <strong>MUST</strong> be called before accessing the addressbook
   */
  public abstract void connect() throws IOException;

  /**
   * Get an address card from a given referance.
   * @throws java.io.IOException
   */
  public abstract AddressCard getAddressCard(String ref)
    throws IOException;

  /**
   * Get a list of address cards matching the given query statement.
   * @throws java.io.IOException If something goes wrong.
   */
  public abstract AddressCardList getAddressCardList(QueryStatement qs)
    throws IOException;

  /**
   * Get a list of address cards matching the given filter.
   * @param filter A string representing a filter, the syntax of the filter is implementation dependant.
   *
   * <em><strong>DO NOT</strong> call this method unless you are sure of what you are doing.</em><br>
   * <em><strong>DO NOT</strong> call this method with user imput.</em>
   * @throws java.io.IOException If something goes wrong.
   */
  public abstract AddressCardList getAddressCardList(String filter)
    throws IOException;

  /**
   * @return true if this address book is connected.
   */
  public abstract boolean isConnected();

  /**
   * A string representing this address book.
   */
  public abstract String toString();
}
