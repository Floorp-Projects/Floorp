/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 *               Mauro Botelho <mabotelh@bellsouth.net>
 */

package grendel.addressbook;

import grendel.addressbook.addresscard.*;
import grendel.ui.UIAction;
import grendel.ui.GeneralFrame;
import grendel.widgets.CollapsiblePanel;
import grendel.widgets.GrendelToolBar;
import calypso.util.QSort;
import calypso.util.Comparer;

import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.util.*;
import java.util.Vector;

import javax.swing.*;
import javax.swing.text.*;
import javax.swing.ImageIcon;
import javax.swing.JPanel;
import javax.swing.table.*;
//import javax.swing.table.DefaultTableModel;
import javax.swing.event.TableModelEvent;
import javax.swing.border.EmptyBorder;

import netscape.ldap.*;

/**
 *
 * @author  Lester Schueler
 */
public class AddressBook extends GeneralFrame {
  private Hashtable         mCommands;
  private Hashtable         mMenuItems;
  //   private ACS_Personal      myLocalAddressBook;

  private JMenuBar          mMenubar;
  private GrendelToolBar    mTtoolbar;
  //    private Component       mStatusbar;
  private JTable            mTable;
  private JButton           mSearchButton;
  protected DataSourceList  mDataSourceList;
  protected JComboBox       mSearchSource;
  protected JTextField      mSearchField;
  protected boolean         mSortAscending;
  protected String          ColumnName;
  protected int             mColumnSorted;

  public static void main(String[] args) {
    AddressBook AddressBookFrame = new AddressBook();
    AddressBookFrame.addWindowListener(new AppCloser());
    AddressBookFrame.show();
  }

  protected static final class AppCloser extends WindowAdapter {
    public void windowClosing(WindowEvent e) {
      System.exit(0);
    }
  }

  //***************************
  abstract class DataSource  {
    private String  mReadableName;
    private ICardSource mCardSource;

    DataSource (String aReadableName) {
      mReadableName = aReadableName;
    }

    private DataSource () {
       mReadableName = "";
    }

    protected abstract ICardSource getCardSource();
    public String            getReadableName ()  { return mReadableName; }
    public Vector   query (String aSearchString) {
       Vector retVecVec = new Vector();   //return vector of vectors.
   
       //              try {
       //open a connection to the LDAP server
       System.out.println ("Opening server " + mReadableName);
       ICardSource Four11AddressBook = getCardSource();
   
   
       //create the query
       ITerm query = new TermEqual (new AC_Attribute ("sn", aSearchString));
   
       String[] attributes = {"givenName", "sn", "cn", "o", "mail", "telephoneNumber", "city"};
   
       //query the LDAP server.
       System.out.println ("Send query" + query);
       ICardSet cardSet = Four11AddressBook.getCardSet (query, attributes);
   
       //Sort the list.
       String[] sortOrder = {"sn", "cn"};
       cardSet.sort (sortOrder);
   
       //hack. I've put the for loop in a try block to catch the exception
       //thrown when cardEnum.hasMoreElements() incorrectly returns true.
       try {
         //enumerate thru the cards.
         for (Enumeration cardEnum = cardSet.getEnumeration(); 
              cardEnum.hasMoreElements(); ) {
           System.out.println ("got card");
           //get the addres card
           ICard card = (ICard) cardEnum.nextElement(); 
           //get the attributes for this card
           IAttributeSet attrSet = card.getAttributeSet ();  
           //create a simple vector to hold the attributes values for this card.
           Vector thisRow = new Vector(6);                     
   
           String commonName = "";
           String organization = "";
           String mail = "";
           String phone = "";
           String city = "";
           String nickName = "";
           
           // enumerate thru the card attributes.
           for (Enumeration attEnum = attrSet.getEnumeration(); 
                attEnum.hasMoreElements(); ) {
             IAttribute attr = (IAttribute) attEnum.nextElement();
             String attrName = attr.getName();
   
             if (attrName.equals ("cn")) {
               commonName = attr.getValue();
             }
   
             else if (attrName.equals ("o")) {
               organization = attr.getValue();
             }
   
             else if (attrName.equals ("mail")) {
               mail = attr.getValue();
             }
   
             else if (attrName.equals ("telephoneNumber")) {
               phone = attr.getValue();
             }
   
             else if (attrName.equals ("city")) {
               city = attr.getValue();
             }
   
           }
   
           //create this row for the table.
           thisRow.addElement (commonName);
           thisRow.addElement (mail);
           thisRow.addElement (organization);
           thisRow.addElement (phone);
           thisRow.addElement (city);
           thisRow.addElement (nickName);
   
           //add this row to the table
           retVecVec.addElement (thisRow);
         }
       }
       catch (Exception e) {
       }
       //              }
       //              catch( LDAPException e ) {
       //                      System.out.println( "Error: " + e.toString() );
       //              }
   
       System.out.println ("Done.");
       return retVecVec;
    }
  }

  //***************************
  class LDAPDataSource extends DataSource {
    private String  mDomainName;
    private int     mPort;

    LDAPDataSource (String aReadableName, String aDomainName) {
      this (aReadableName, aDomainName, 389);
    }

    LDAPDataSource (String aReadableName, String aDomainName, int aPort) {
      super(aReadableName);
      mDomainName     = aDomainName;
      mPort           = aPort;
    }

    protected ICardSource getCardSource()     {
       return new LDAP_Server (mDomainName);
    }
    public String   getDomainName ()    { return mDomainName; }
    public int      getPort ()          { return mPort; }
  }
  //***************************
  class FileDataSource extends DataSource {
    private String  mFileName;

    FileDataSource (String aReadableName, String aFileName) {
      super(aReadableName);
      mFileName = aFileName;
    }

    protected ICardSource getCardSource() {
       System.out.println("Entering in the getCardSource");
       return new ACS_Personal (mFileName, false);
    }
    public String   getFileName ()  { return mFileName; }
       
  }

  //***************************
  class DataSourceList {
    private Vector mDataSources;

    DataSourceList () {
      mDataSources = new Vector ();
    }

    public Enumeration getEnumeration () { return mDataSources.elements(); }
    public void addEntry (DataSource aDataSource) { mDataSources.addElement (aDataSource); }

    public DataSource find (String aReadableName) {
      for (Enumeration e = mDataSources.elements() ; e.hasMoreElements() ;) {
        DataSource ds = (DataSource) e.nextElement();

        if (ds.getReadableName ().equalsIgnoreCase(aReadableName))
          return ds;
      }

      return null;
    }
  }

  /**
   *
   */
  public AddressBook() {
    super("Address Book","0");

    // Setting the default values to the variables
    mSortAscending = true;
    mColumnSorted = 0;

    setBackground(Color.lightGray);
    getContentPane().setLayout(new BorderLayout());


    //create menubar (top)
    //merge both the editors commands with this applications commands.
    //mMenubar = NsMenuManager.createMenuBar("grendel.addressbook.Menus", "grendel.addressbook.MenuLabels", "mainMenubar", defaultActions);
    // FIXME - need to build the menu bar
    // (Jeff)

    mMenubar = buildMenu("menus.xml",defaultActions);

    Component[] menulist = mMenubar.getComponents();
    for(int j = 0 ; j < menulist.length ; j++) {
       System.out.println(menulist[j].getName());
    }

    setJMenuBar(mMenubar);

    //collapsble panels holds toolbar.
    CollapsiblePanel collapsePanel = new CollapsiblePanel(true);
    collapsePanel.setBorder (new EmptyBorder(5,5,5,5));

    //toolbar buttons
    mTtoolbar = createToolbar();

    //collapsible item
    collapsePanel.add(mTtoolbar);

    //create status bar (bottom)
    //        mStatusbar = createStatusbar();

    JPanel panel1 = new JPanel();
    panel1.setLayout(new BorderLayout());
    panel1.add(collapsePanel, BorderLayout.NORTH);

    //hack together the data sources.
    mDataSourceList = new DataSourceList ();
    mDataSourceList.addEntry (new FileDataSource ("Local Addressbook","aBook.nab"));
    mDataSourceList.addEntry (new LDAPDataSource ("Four11 Directory", "ldap.four11.com"));
    mDataSourceList.addEntry (new LDAPDataSource ("InfoSpace Directory", "ldap.infospace.com"));
    mDataSourceList.addEntry (new LDAPDataSource ("WhoWhere Directory", "ldap.whowhere.com"));
    mDataSourceList.addEntry (new LDAPDataSource ("InfoSeek Directory", "ldap.infoseek.com"));

    //Create address panel
    AddressPanel addressPanel = new AddressPanel (mDataSourceList);
    panel1.add(addressPanel, BorderLayout.CENTER);

    // getContentPane().add(mMenubar, BorderLayout.NORTH);
    getContentPane().add(panel1, BorderLayout.CENTER);

    setSize (600, 400);

    //Create Local Address Book
    //myLocalAddressBook = new ACS_Personal ("MyAddressBook.nab", true);

  }

  /**
   * Hide this frame.
   */
  protected void hideThisFrame () {
    setVisible(false);
  }

  /**
   * Handles windowClosing for window listener.
   */
  protected class FrameHider extends WindowAdapter {
    public void windowClosing(WindowEvent e) {
      hideThisFrame();
    }
  }

  /**
   * Find the hosting frame, for the file-chooser dialog.
   */
  protected JFrame getParentFrame() {
    //        for (Container p = getParent(); p != null; p = p.getParent()) {
    //            if (p instanceof Frame) {
    //              return (Frame) p;
    //            }
    //        }
    return this;
  }

  //**********************
  //**********************
  // menus
  private JMenuBar menubar;

  //"File" actions
  public static final String newCardTag               ="newCard";
  public static final String newListTag               ="newList";
  public static final String importTag                ="import";
  public static final String saveAsTag                ="saveAs";
  public static final String callTag                  ="call";
  public static final String closeWindowTag           ="closeWindow";

  // "file->new" actions
  public static final String navigatorWindowTag       ="navigatorWindow";
  public static final String messageTag               ="message";
  public static final String blankPageTag             ="blankPage";
  public static final String pageFromTemplateTag      ="pageFromTemplate";
  public static final String pageFromWizardTag        ="pageFromWizard";

  //"Edit" actions
  public static final String undoTag                  ="undo";
  public static final String redoTag                  ="redo";
  public static final String deleteTag                ="delete";
  public static final String searchDirectoryTag       ="searchDirectory";
  public static final String HTMLDomainsTag           ="HTMLDomains";
  public static final String cardPropertiesTag        ="cardProperties";
  public static final String preferencesTag           ="preferences";

  //"View" actions
  public static final String hideMessageToolbarTag    ="hideMessageToolbar";
  public static final String byTypeTag                ="byType";
  public static final String byNameTag                ="byName";
  public static final String byEmailAddressTag        ="byEmailAddress";
  public static final String byCompanyTag             ="byCompany";
  public static final String byCityTag                ="byCity";
  public static final String byNicknameTag            ="byNickname";
  public static final String sortAscendingTag         ="sortAscending";
  public static final String sortDescendingTag        ="sortDescending";
  public static final String myAddressBookCardTag     ="myAddressBookCard";

  // --- action implementations -----------------------------------
  private UIAction[] defaultActions = {
    //"File" actions
    new NewCard(),
    //        new NewList(),
    //        new Import(),
    new SaveAs(),
    //        new Call(),
    new CloseWindow(),

    // "file->new" actions
    //        new NavigatorWindow(),
    //        new Message(),
    //        new BlankPage(),
    //        new PageFromTemplate(),
    //        new PageFromWizard(),

    //"Edit" actions
    //        new Undo(),
    //        new Redo(),
    //        new Delete(),
    new SearchDirectory(),
    //        new HTMLDomains(),
    //        new CardProperties(),
    //        new Preferences(),

    //"View" actions
    //        new HideMessageToolbar(),
    //        new ByType(),
    new ByName(),
    new ByEmailAddress(),
    new ByCompany(),
    new ByCity(),
    new ByNickname(),
    new SortAscending(),
    new SortDescending(),
    // new MyAddressBookCard(),
    // new WrapLongLines()
  };

  //-----------------------
  //"File" actions
  //-----------------------
  /**
   */
  class NewCard extends UIAction {
    NewCard() {
      super(newCardTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      NewCardDialog aDialog = new NewCardDialog(getParentFrame());

      //display the new card dialog
      aDialog.show ();
      aDialog.dispose();
    }
  }

  class SearchDirectory extends UIAction {
    SearchDirectory() {
      super(searchDirectoryTag);
      setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      SearchDirectoryDialog aDialog = new SearchDirectoryDialog(getParentFrame());

      //display the new card dialog
      aDialog.show ();
      aDialog.dispose();
    }
  }


  class SaveAs extends UIAction {
    SaveAs() {
      super(saveAsTag);
      this.setEnabled(true);
    }
    public void actionPerformed(ActionEvent ae) {
      NewCardDialog aDialog = new NewCardDialog(getParentFrame());

      //display the new card dialog
      aDialog.show ();
      aDialog.dispose();
    }
  }

  class CloseWindow extends UIAction {
    CloseWindow() {
      super(closeWindowTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      //hide after send.
      hideThisFrame();
    }
  }

  //-----------------------
  // "file->new" actions
  //-----------------------

  //-----------------------
  //"Edit" actions
  //-----------------------
  class Undo extends UIAction {
    Undo() {
      super(undoTag);
      this.setEnabled(true);
    }
    public void actionPerformed(ActionEvent e) {}
  }

  //-----------------------
  //"View" actions
  //-----------------------
  class HideMessageToolbar extends UIAction {
    HideMessageToolbar() {
      super(hideMessageToolbarTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {}
  }

  //-----------------------
  //-----------------------
  class Search extends UIAction {
    Search() {
      super(newListTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      String readableName = (String) mSearchSource.getSelectedItem();
      int index = mSearchSource.getSelectedIndex();

      DataSource ds = mDataSourceList.find (readableName);
      if (null != ds) {

        //get the text to search for.
        String textToSearchFor = mSearchField.getText();

        if (!textToSearchFor.trim().equals ("")) {
          DataModel dm = (DataModel) mTable.getModel ();
          dm.reloadData (ds, textToSearchFor);
          //dm.reloadData (ds.getDomainName(), ds.getPort(), textToSearchFor);

          dm.fireTableDataChanged();

          //repaint the table with results.
          mTable.repaint();
        }
      }
    }
  }
    //----------------
    // Sort Ascending
    //----------------
    class SortAscending extends UIAction {
        SortAscending() {
            super(sortAscendingTag);
            this.setEnabled(true);
        }
        public void actionPerformed(ActionEvent e) {
          if (!mSortAscending) {
            mSortAscending = true;
            DataModel dm = (DataModel) mTable.getModel ();
            System.out.println("Column Name is " + ColumnName);
            int colnumber = dm.findColumn(ColumnName);
            System.out.println("Column Number for " + ColumnName + " is: " + colnumber);

            dm.sortData(colnumber,mSortAscending);

            mTable.repaint();
          }
        }
    }


    //----------------
    // Sort Descending
    //----------------
    class SortDescending extends UIAction {
        SortDescending() {
            super(sortDescendingTag);
            this.setEnabled(true);
        }
        public void actionPerformed(ActionEvent e) {
          System.out.println("I'm in sort descending");
          if (mSortAscending) {
            DataModel dm = (DataModel) mTable.getModel ();
            mSortAscending = false;
            System.out.println("Column Name is " + ColumnName);
            int colnumber = dm.findColumn(ColumnName);
            System.out.println("Column Number for " + ColumnName + " is: " + colnumber);

            dm.sortData(colnumber,mSortAscending);

            mTable.repaint();
          }
        }
    }

    //----------------------------------
    // Base class for sorting the names
    //----------------------------------
    class ResultSorter extends UIAction {
        String myLocalColumnName;
        ResultSorter(String Tag){
          super(Tag);
        }

        public void actionPerformed(ActionEvent e) {
            DataModel dm = (DataModel) mTable.getModel ();
            ColumnName = myLocalColumnName;

            int colnumber = dm.findColumn(ColumnName);
            System.out.println("Column Number for " + ColumnName + " is: " + colnumber);

            dm.sortData(colnumber,mSortAscending);

            mTable.repaint();
        }
    }
    //-----------------------
    //ByName action
    //-----------------------

    class ByName extends ResultSorter {
        ByName() {
            super(byNameTag);
            myLocalColumnName = new String("Name");
            this.setEnabled(true);
        }

    }

    //-----------------------
    //ByEmailAddress action
    //-----------------------

    class ByEmailAddress extends ResultSorter {
        ByEmailAddress() {
            super(byEmailAddressTag);
            myLocalColumnName = new String("Email Address");
            this.setEnabled(true);
        }
    }

    //-----------------------
    //ByCompany action
    //-----------------------

    class ByCompany extends ResultSorter {
        ByCompany() {
            super(byCompanyTag);
            myLocalColumnName = new String("Organization");
            this.setEnabled(true);
        }
    }

    //-----------------------
    //ByCity action
    //-----------------------
    class ByCity extends ResultSorter {
        ByCity() {
            super(byCityTag);
            myLocalColumnName = new String("City");
            this.setEnabled(true);
        }
    }

    //-----------------------
    //ByNickname action
    //-----------------------

    class ByNickname extends ResultSorter {
        ByNickname() {
            super(byNicknameTag);
            myLocalColumnName = new String("Nickname");
            this.setEnabled(true);
        }
    }

  /**
   * Create a Toolbar
   * @see addToolbarButton
   */
  private GrendelToolBar createToolbar() {

    GrendelToolBar toolBar = new GrendelToolBar();
    addToolbarButton(toolBar, new NewCard(),     "images/newcard.gif",       "Create a new card");
    addToolbarButton(toolBar, null,     "images/newlist.gif",       "Create a new list");
    addToolbarButton(toolBar, null,     "images/properties.gif",    "Edit the selected card");
    addToolbarButton(toolBar, null,     "images/newmsg.gif",        "New Message (Ctrl+M)");
    addToolbarButton(toolBar, null,     "images/directory.gif",     "Look up an address");
    addToolbarButton(toolBar, null,     "images/call.gif",          "Start Netscape Conference");
    addToolbarButton(toolBar, null,     "images/delete.gif",        "Delete selected cards <Del>");

    return toolBar;
  }

  /**
   * create a toolbar button
   * @param aToolBar The parent toolbar to add this button to.
   * @param aActionListener Who you want to be notified when the button is pressed.
   * @param aImageName The image name for the button. like "save.gif"
   * @param aToolTip The buttons tool tip. like "Save the current file".
   * @see createToolbar
   */
  public void addToolbarButton(GrendelToolBar aToolBar, 
                               UIAction aActionListener, 
                               String aImageName, String aToolTip) {
    JButton b = new JButton();

    b.setHorizontalTextPosition(JButton.CENTER);
    b.setVerticalTextPosition(JButton.BOTTOM);
    b.setToolTipText(aToolTip);

    //        URL iconUrl = getClass().getResource("images/" + gifName + ".gif");
    //        b.setIcon(new ImageIcon(getClass().getResource(aImageName)));

    // FIXME - need the toolbar graphics for this sub-app
    // (Jeff)

    b.setIcon(new ImageIcon(getClass().getResource("markAllRead.gif")));
    //    iconUrl = getClass().getResource("images/" + gifName + "-disabled.gif");
    //    button.setDisabledIcon(ImageIcon.createImageIcon(iconUrl));

    //    iconUrl = getClass().getResource("images/" + gifName + "-depressed.gif");
    //    button.setPressedIcon(ImageIcon.createImageIcon(iconUrl));

    //    iconUrl = getClass().getResource("images/" + gifName + "-rollover.gif");
    //    button.setRolloverIcon(ImageIcon.createImageIcon(iconUrl));


    //        JButton b = new JButton(new ImageIcon(aImageName));
    b.setToolTipText(aToolTip);
    //      b.setPad(new Insets(3,3,3,3));
    if (aActionListener != null) {
      b.addActionListener(aActionListener);}
    
    aToolBar.add(b);
  }

  private String getFirstEnum (Enumeration enumVals) {
    if (enumVals.hasMoreElements())
      return (String) enumVals.nextElement();

    return null;
  }

  /* This function is now obsolete, the responsible for querying is now
  ** the datasource, so that we can create other types of datasources (files!)
  public Vector queryLDAP (String aServerName, int aPort, 
                           String aSearchString) {

    Vector retVecVec = new Vector();   //return vector of vectors.

    //              try {
    //open a connection to the LDAP server
    System.out.println ("Opening server " + aServerName);
    ICardSource Four11AddressBook = new LDAP_Server (aServerName);


    //create the query
    ITerm query = new TermEqual (new AC_Attribute ("sn", aSearchString));

    String[] attributes = {"sn", "cn", "o", "mail", "city"};

    //query the LDAP server.
    System.out.println ("Send query" + query);
    ICardSet cardSet = Four11AddressBook.getCardSet (query, attributes);

    //Sort the list.
    String[] sortOrder = {"sn", "cn"};
    cardSet.sort (sortOrder);

    //hack. I've put the for loop in a try block to catch the exception
    //thrown when cardEnum.hasMoreElements() incorrectly returns true.
    try {
      //enumerate thru the cards.
      for (Enumeration cardEnum = cardSet.getEnumeration(); 
           cardEnum.hasMoreElements(); ) {
        System.out.println ("got card");
        //get the addres card
        ICard card = (ICard) cardEnum.nextElement(); 
        //get the attributes for this card
        IAttributeSet attrSet = card.getAttributeSet ();  
        //create a simple vector to hold the attributes values for this card.
        Vector thisRow = new Vector(6);                     

        String commonName = "";
        String organization = "";
        String mail = "";
        String phone = "";
        String city = "";
        String nickName = "";
        
        // enumerate thru the card attributes.
        for (Enumeration attEnum = attrSet.getEnumeration();
             attEnum.hasMoreElements(); ) {
          IAttribute attr = (IAttribute) attEnum.nextElement();
          String attrName = attr.getName();

          if (attrName.equals ("cn")) {
            commonName = attr.getValue();
          }

          else if (attrName.equals ("o")) {
            organization = attr.getValue();
          }

          else if (attrName.equals ("mail")) {
            mail = attr.getValue();
          }

          else if (attrName.equals ("sn")) {
            phone = attr.getValue();
          }

          else if (attrName.equals ("city")) {
            city = attr.getValue();
          }

        }

        //create this row for the table.
        thisRow.addElement (commonName);
        thisRow.addElement (mail);
        thisRow.addElement (organization);
        thisRow.addElement (phone);
        thisRow.addElement (city);
        thisRow.addElement (nickName);

        //add this row to the table
        retVecVec.addElement (thisRow);
      }
    }
    catch (Exception e) {
    }
    //              }
    //              catch( LDAPException e ) {
    //                      System.out.println( "Error: " + e.toString() );
    //              }

    System.out.println ("Done.");
    return retVecVec;
  }
  */

  public class DataModel extends AbstractTableModel {
    private Vector mVecVec;
    private String[] mColumnNames;

    public DataModel (String[] aNames) {
      super();
      mVecVec = new Vector ();
      mColumnNames = aNames;
    }

    public int      getRowCount()                       { return mVecVec.size(); }
    public int      getColumnCount ()                   { return mColumnNames.length; }
    public String   getColumnName(int column)           { return mColumnNames[column]; }
    public Class    getColumnClass(int col)             { return String.class; }
    public boolean  isCellEditable(int row, int col)    { return false;}

    public void setValueAt(Object aValue, int row, int column) {
      ((Vector)mVecVec.elementAt(row)).setElementAt(aValue, column);
    }

    public void reloadData (DataSource ds, String aSearchString) {
       mVecVec = ds.query(aSearchString);
    }

    /* This method is now obsolete, since I moved the query function
    ** from the AddressBook to the datasource
    **
    public void reloadData (String aServerName, int aPort, 
                            String aSearchString) {
      //reload the data from LDAP.
      mVecVec = queryLDAP (aServerName, aPort, aSearchString);
    }
   */

    public Object getValueAt(int row, int column) {
      return (((Vector)mVecVec.elementAt(row)).elementAt(column));
    }

    public void sortData(int column, final boolean sortascending) {
      Object[][] ColumnValues = new String [mVecVec.size()][2];
      int row;
      int col;

      for(row = 0 ; row < mVecVec.size() ; ++row) {
        ColumnValues[row][0] =
          (((Vector)mVecVec.elementAt(row)).elementAt(column));
        ColumnValues[row][1] = new Integer(row).toString();
      }

      System.out.println("Values before sorting");
      for(row = 0; row < ColumnValues.length ; ++row) {
        System.out.println(ColumnValues[row][0] + " row: "
                           + ColumnValues[row][1]);
      }

      QSort sorter = new QSort(new Comparer() {
        public int compare(Object a, Object b) {
          int returnValue;
          try {
            returnValue = (((String[])a)[0]).compareTo(((String[])b)[0]);
          }
          catch (NullPointerException e) {
            returnValue = 1;
          }
          if (!sortascending)
            returnValue = -1 * returnValue;
          return returnValue;
        }
      });
      sorter.sort(ColumnValues);

      System.out.println("Values after sorting");
      for(row = 0; row < ColumnValues.length ; ++row) {
        System.out.println(ColumnValues[row][0] + " row: "
                           + ColumnValues[row][1]);
      }

      Vector SortedVector = new Vector();

      for(row = 0; row < ColumnValues.length ; ++row) {
        Vector thisRow = new Vector(6);
        Integer OriginalPosition = new Integer((String)ColumnValues[row][1]);
        for(col = 0; col < ((Vector)mVecVec.elementAt(row)).size();
            ++col) {
          thisRow.addElement(((Vector)mVecVec.elementAt(OriginalPosition.intValue())).elementAt(col));
        }
        SortedVector.addElement(thisRow);
      }

      mVecVec = SortedVector;

      fireTableDataChanged();
    }
  }

  /**
   * Create a status bar
   * @return
   * @see
   */
  //    protected Component createStatusbar() {
  //      // need to do something reasonable here
  //          StatusBar status = new StatusBar();
  //      return status;
  //    }

  //***************************
  /**
   *
   */
    class StatusBar extends JPanel {
      public StatusBar() {
        super();
        this.setLayout(new BoxLayout(this, BoxLayout.X_AXIS));
      }

      public void paint(Graphics g) {
        super.paint(g);
      }
    }

  /**
   */
  class AddressPanel extends JPanel {
    
    public AddressPanel(DataSourceList aDataSourceList) {
      //super(true);
      
      setBorder (new EmptyBorder(10,10,10,10));
      this.setLayout (new BorderLayout(10, 5));
      
      add(createSearchPane(aDataSourceList), BorderLayout.NORTH);
      add(createTable(),      BorderLayout.CENTER);
    }
    
    private Box createSearchPane (DataSourceList aDataSourceList) {
      //explaination
      JLabel explaination = new JLabel ("Type in the name you're looking for:");
      explaination.setAlignmentX((float)0.0);        //align left
      
      //text field
      mSearchField = new JTextField (20);
      mSearchField.setAlignmentX((float)0.0);        //align left
      
      //box for explain and text field
      Box innerBoxPane = new Box (BoxLayout.Y_AXIS);
      //            innerBoxPane.setAlignmentY((float)0.0);        //align to bottom
      innerBoxPane.add (explaination);
      innerBoxPane.add (mSearchField);
      
      //drop down combo box
      mSearchSource = new JComboBox();
      mSearchSource.setAlignmentY((float)0.0);        //align to bottom
      for (Enumeration e = aDataSourceList.getEnumeration() ; 
           e.hasMoreElements() ;) {
        DataSource ds = (DataSource) e.nextElement();
        mSearchSource.addItem(ds.getReadableName());
      }
      
      //label
      JLabel lbl = new JLabel ("in:");
      lbl.setAlignmentY((float)0.0);        //align to bottom
      
      //search button
      mSearchButton = new JButton ("Search");
      mSearchButton.addActionListener(new Search());
      mSearchButton.setAlignmentY((float)0.0);         //align to bottom
      
      Dimension spacer = new Dimension (10, 10);
      
      //assemble all the pieces together.
      Box boxPane = new Box (BoxLayout.X_AXIS);
      
      boxPane.add (innerBoxPane);                 //explaination and text field
      boxPane.add (Box.createRigidArea(spacer));  //spacer
      boxPane.add (lbl);                          //"in:" label
      boxPane.add (Box.createRigidArea(spacer));  //spacer
      boxPane.add (mSearchSource);                //drop down combo box
      boxPane.add (Box.createRigidArea(spacer));  //spacer
      boxPane.add (mSearchButton);                //search buttton
      
      return boxPane;
    }
    
    private JScrollPane createTable () {
      String[] columnNames = {
        "Name",
        "Email Address",
        "Organization",
        "Phone",
        "City",
        "Nickname"};
      
      //create the data model.
      DataModel dm = new DataModel (columnNames);
      
      //create the table.
      mTable = new JTable(dm);
      
      //              mTable.setAutoCreateColumnsFromModel(false);
      
      // Add our columns into the  column model
      //            for (int columnIndex = 0; columnIndex < columnNames.length; columnIndex++){
      // Create a column object for each column of data
      //                TableColumn newColumn = new TableColumn(columnNames[columnIndex]);
      
      // Set a tool tip for the column header cell
      //              TableCellRenderer renderer2 = newColumn.getHeaderRenderer();
      //              if (renderer2 instanceof DefaultCellRenderer)
      //                    ((DefaultCellRenderer)renderer2).setToolTipText(columnNames[columnIndex]);
      
      //                newColumn.setWidth(200);
      //                mTable.addColumn(newColumn);
      //            }
      
      //no selection, no grid.
      mTable.setColumnSelectionAllowed(false);
      mTable.setShowGrid(false);
      
      // Put the table and header into a scrollPane
      JScrollPane scrollpane = new JScrollPane(mTable);
      //            JTableHeader tableHeader = mTable.getTableHeader();
      
      // create and add the column heading to the scrollpane's
      // column header viewport
      //            JViewport headerViewport = new JViewport();
      //            headerViewport.setLayout(new BoxLayout(headerViewport, BoxLayout.X_AXIS));
      //            headerViewport.add(tableHeader);
      //            scrollpane.setColumnHeader(headerViewport);
      
      // add the table to the viewport
      ///            JViewport mainViewPort = scrollpane.getViewport();
      //            mainViewPort.add(mTable);
      //            mainViewPort.setBackground (Color.white);
      
      // speed up resizing repaints by turning off live cell updates
      //            tableHeader.setUpdateTableInRealTime(false);
      
      //return the JScrollPane with the table in it.
      return scrollpane;
    }
  }
}
