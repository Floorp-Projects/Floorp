/*
 * AddressBook.java
 *
 * Created on 23 September 2005, 22:19
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook.jdbc;

import grendel.addressbook.AddressBook;
import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressBookFeildValueList;
import grendel.addressbook.AddressCard;
import grendel.addressbook.AddressCardList;

import grendel.addressbook.query.QueryStatement;

import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;

import java.io.IOException;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.Map;
import java.util.Set;


/**
 *
 * @author hash9
 */
public abstract class JDBCAddressBook extends AddressBook {
    static
    {
        JDBCRegister.registerJDBCdrivers();
    }
    
    protected Connection connection;
    
    public AddressCardList getAddressCardList(QueryStatement qs)
    throws IOException {
        String sql_where = null;
        
        if (qs != null) {
            sql_where = qs.makeSQL();
        }
        
        return getAddressCardList(sql_where);
    }
    
    public AddressCardList getAddressCardList(String filter)
    throws IOException {
        String sql = "select * from "+getTable();
        
        if (filter != null) {
            sql = sql.concat(" WHERE ").concat(filter);
        }
        
        try {
            return new JDBCAddressCardList(runSQL(sql));
        } catch (SQLException ex) {
            NoticeBoard.publish(new ExceptionNotice(ex)); //XXX Should be wrapped as a IOException
        }
        
        return null; //XXX see above
    }
    
    public abstract String getTable();
    public abstract String getIndexTable();
    
    private int getNextID() throws SQLException {
        ResultSet rs = runSQL("select id from "+getIndexTable());
        rs.last();
        int id = rs.getInt("id");
        id++;
        return id;
    }
    
    public void addNewCard(AddressCard ac)
    throws IOException {
        try {
            String pre = "INSERT INTO " + getTable() + "VALUES (";
            String post = ");";
            String id = Integer.toString(getNextID());
            
            { // Insert Values into main table : ID FEILD VALUE FLAGS
                Map<String,AddressBookFeildValueList> all_values = ac.getAllValues();
                Set<String> keys = all_values.keySet();
                for (String key: keys) {
                    AddressBookFeildValueList vals = all_values.get(key);
                    for (AddressBookFeildValue val: vals) {
                        addValue(id, key, val);
                    }
                }
            }
            
            // Update index table : // ID NAME EMAIL
            {
                StringBuilder sb = new StringBuilder(pre);
                sb.append("'");
                sb.append(id);
                sb.append("', '");
                sb.append(ac.getName());
                sb.append("', '");
                sb.append(ac.getEMail());
                sb.append("'");
                sb.append(post);
                execSQL(sb.toString());
            }
        } catch (SQLException sqle) {
            sqle.printStackTrace(); //XXX fix this!!!
            throw new IOException(sqle.getMessage());
        }
    }
    
    void addValue(int id, String key, AddressBookFeildValue value) throws SQLException {
        addValue(Integer.toString(id),key,value);
    }
    
    private void addValue(String id, String key, AddressBookFeildValue val) throws SQLException{
        String pre = "INSERT INTO " + getTable() + "VALUES (";
        String post = ");";
        StringBuilder sb = new StringBuilder(pre);
        sb.append("'");
        sb.append(id);
        sb.append("', '");
        sb.append(key);
        sb.append("', '");
        sb.append(val.getValue());
        sb.append("', '");
        if (val.isHidden()) sb.append("h");
        if (val.isReadonly()) sb.append("r");
        if (val.isSystem()) sb.append("s");
        sb.append("'");
        sb.append(post);
        execSQL(sb.toString());
    }
    
    private void execSQL(String sql) throws SQLException {
        Statement s = connection.createStatement();
        s.executeUpdate(sql);
    }
    
    private ResultSet runSQL(String sql) throws SQLException {
        Statement s = connection.createStatement();
        return s.executeQuery(sql);
    }
    
    public AddressCard getAddressCard(String ref) throws IOException {
        String filter = "\"id\"=" + ref;
        AddressCardList list = getAddressCardList(filter);
        
        if ((list == null)|| (list.size() == 0)) {
            return null;
        }
        
        return list.get(0);
    }
    
    public boolean isConnected() {
        try {
            if (connection == null) {
                return false;
            }
            
            return (!connection.isClosed());
        } catch (SQLException ex) {
            ex.printStackTrace(); //XXX fix this!!!
            
            return false;
        }
    }

    
}
