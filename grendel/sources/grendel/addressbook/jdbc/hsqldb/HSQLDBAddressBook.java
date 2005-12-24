/*
 * HSQLDBAddressBook.java
 *
 * Created on 15 October 2005, 22:56
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc.hsqldb;

import grendel.addressbook.jdbc.JDBCAddressBook;

import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;

import grendel.prefs.addressbook.Addressbook_HSQLDB;

import java.io.IOException;

import java.sql.SQLException;


/**
 *
 * @author hash9
 */
public class HSQLDBAddressBook extends JDBCAddressBook
{
  private Addressbook_HSQLDB addressbook;

  /**
   * Creates a new instance of Addressbook_HSQLDB
   */
  public HSQLDBAddressBook(Addressbook_HSQLDB addressbook)
  {
    this.addressbook = addressbook;
  }

  public void connect() throws IOException
  {
    String connection_str = "jdbc:hsqldb:file:" + addressbook.getDirectory();

    try
    {
      connection = java.sql.DriverManager.getConnection(connection_str);
    }
    catch (SQLException ex)
    {
      NoticeBoard.publish(new ExceptionNotice(ex));
    }
  }

  public String toString()
  {
    return addressbook.getName();
  }

    public String getTable() {
        return "\"Addressbook\"";
    }

    public String getIndexTable() {
        return "\"AddressBookIndex\"";
    }
}
