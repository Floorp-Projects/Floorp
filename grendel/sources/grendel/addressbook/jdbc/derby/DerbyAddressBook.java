/*
 * AddressBook.java
 *
 * Created on 23 September 2005, 22:19
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc.derby;

import grendel.addressbook.AddressCard;
import grendel.addressbook.AddressCardList;

import grendel.addressbook.jdbc.JDBCAddressBook;
import grendel.addressbook.jdbc.JDBCAddressCardList;

import grendel.addressbook.query.QueryStatement;

import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.messaging.StringNotice;

import grendel.prefs.addressbook.Addressbook_Derby;

import java.io.IOException;

import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;


/**
 *
 * @author hash9
 */
public class DerbyAddressBook extends JDBCAddressBook
{
  static
  {
    System.setProperty("derby.system.home",
      Addressbook_Derby.getBaseDirectory().concat("derby"));
    System.setProperty("derby.infolog.append", "true");
  }

  private Addressbook_Derby addressbook;

  /**
   * Creates a new instance of DerbyAddressBook
   */
  public DerbyAddressBook(Addressbook_Derby addressbook)
  {
    this.addressbook = addressbook;
  }

  public void connect() throws IOException
  {
    String connection_str = "jdbc:derby:" + addressbook.getSimpleName();

    try
    {
      connection = java.sql.DriverManager.getConnection(connection_str);
    }
    catch (SQLException ex)
    {
      connection_str += ";create=true";
      NoticeBoard.publish(new StringNotice(
          "WARNING:: Derby Address Book Database: " +
          addressbook.getSimpleName() + " was created!"));

      try
      {
        connection = DriverManager.getConnection(connection_str);
      }
      catch (SQLException ex1)
      {
        NoticeBoard.publish(new ExceptionNotice(ex));
        NoticeBoard.publish(new ExceptionNotice(ex1));
      }
    }
  }

  public String toString()
  {
    return addressbook.getName();
  }

    public String getTable() {
        return  "\"APP\".\"Addressbook\"";
    }

    public String getIndexTable() {
        return  "\"APP\".\"AddressBookIndex\"";
    }
}
