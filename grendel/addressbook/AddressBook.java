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
 */

package grendel.addressbook;

import grendel.addressbook.addresscard.*;
import grendel.ui.UIAction;
import grendel.widgets.CollapsiblePanel;
import grendel.widgets.GrendelToolBar;

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
public class AddressBook extends JFrame {
    private Hashtable       mCommands;
    private Hashtable       mMenuItems;

    private JMenuBar        mMenubar;
    private GrendelToolBar       mTtoolbar;
//    private Component       mStatusbar;
    private JTable          mTable;
    private JButton         mSearchButton;
    protected  DataSourceList  mDataSourceList;
    protected JComboBox       mSearchSource;
    protected  JTextField      mSearchField;

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
    class DataSource {
        private String  mReadableName;
        private String  mDomainName;
        private int     mPort;

        DataSource (String aReadableName, String aDomainName) {
            this (aReadableName, aDomainName, 389);
        }

        DataSource (String aReadableName, String aDomainName, int aPort) {
            mReadableName   = aReadableName;;
            mDomainName     = aDomainName;
            mPort           = aPort;
        }

        public String   getReadableName ()  { return mReadableName; }
        public String   getDomainName ()    { return mDomainName; }
        public int      getPort ()          { return mPort; }
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
        super("Address Book");

        setBackground(Color.lightGray);
        //setBorderStyle(JPanel.ETCHED);
        getContentPane().setLayout(new BorderLayout());

//        addWindowListener(new FrameHider());

        //create menubar (top)
        //merge both the editors commands with this applications commands.
        //        mMenubar = NsMenuManager.createMenuBar("grendel.addressbook.Menus", "grendel.addressbook.MenuLabels", "mainMenubar", defaultActions);
        // FIXME - need to build the menu bar
        // (Jeff)

        mMenubar = new JMenuBar();

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
        mDataSourceList.addEntry (new DataSource ("Four11 Directory", "ldap.four11.com"));
        mDataSourceList.addEntry (new DataSource ("InfoSpace Directory", "ldap.infospace.com"));
        mDataSourceList.addEntry (new DataSource ("WhoWhere Directory", "ldap.whowhere.com"));

        //Create address panel
        AddressPanel addressPanel = new AddressPanel (mDataSourceList);
        panel1.add(addressPanel, BorderLayout.CENTER);

        getContentPane().add(mMenubar, BorderLayout.NORTH);
        getContentPane().add(panel1, BorderLayout.CENTER);

        setSize (600, 400);
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
    protected Frame getParentFrame() {
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
    public static final String byComapanyTag            ="byComapany";
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
        new CloseWindow()

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
//        new SearchDirectory(),
//        new HTMLDomains(),
//        new CardProperties(),
//        new Preferences(),

        //"View" actions
//        new HideMessageToolbar(),
//        new ByType(),
//        new ByName(),
//        new ByEmailAddress(),
//        new ByCompany(),
//        new ByCity(),
//        new ByNickname(),
//        new SortAscending(),
//        new SortDescending(),
//        new MyAddressBookCard(),
//        new WrapLongLines()
    };

    //-----------------------
    //"File" actions
    //-----------------------
    /**
     */
    class NewCard extends UIAction {
        NewCard() {
            super(newCardTag);
            setEnabled(true);
        }

        public void actionPerformed(ActionEvent e) {
            NewCardDialog aDialog = new NewCardDialog(getParentFrame());

            //display the new card dialog
            aDialog.show ();
            aDialog.dispose();
        }
    }

    class SaveAs extends UIAction {
        SaveAs() {
            super(saveAsTag);
            setEnabled(true);
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
            setEnabled(true);
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
            setEnabled(true);
        }
        public void actionPerformed(ActionEvent e) {}
    }

    //-----------------------
    //"View" actions
    //-----------------------
    class HideMessageToolbar extends UIAction {
        HideMessageToolbar() {
            super(hideMessageToolbarTag);
            setEnabled(true);
        }

        public void actionPerformed(ActionEvent e) {}
    }

    //-----------------------
    //-----------------------
    class Search extends UIAction {
        Search() {
            super(newListTag);
            setEnabled(true);
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
                    dm.reloadData (ds.getDomainName(), ds.getPort(), textToSearchFor);

                    dm.fireTableDataChanged();

                    //repaint the table with results.
                    mTable.repaint();
                }
            }
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
    public void addToolbarButton(GrendelToolBar aToolBar, UIAction aActionListener, String aImageName, String aToolTip) {
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

    public Vector queryLDAP (String aServerName, int aPort, String aSearchString) {

        Vector retVecVec = new Vector();   //return vector of vectors.

//              try {
            //open a connection to the LDAP server
System.out.println ("Opening server");
            ICardSource Four11AddressBook = new LDAP_Server (aServerName);

            //create the query
            ITerm query = new TermEqual (new AC_Attribute ("sn", aSearchString));

            String[] attributes = {"sn", "cn", "o", "mail", "city"};

            //query the LDAP server.
System.out.println ("Send query");
            ICardSet cardSet = Four11AddressBook.getCardSet (query, attributes);

            //Sort the list.
//            String[] sortOrder = {"givenname", "surname"};
//            cardSet.sort (sortOrder);

            //hack. I've put the for loop in a try block to catch the exception
            //thrown when cardEnum.hasMoreElements() incorrectly returns true.
            try {
                //enumerate thru the cards.
                for (Enumeration cardEnum = cardSet.getEnumeration(); cardEnum.hasMoreElements(); ) {
System.out.println ("got card");
                    ICard card = (ICard) cardEnum.nextElement();              //get the addres card
                    IAttributeSet attrSet = card.getAttributeSet ();  //get the attributes for this card
                    Vector thisRow = new Vector(6);                     //create a simple vector to hold the attributes values for this card.

                                String commonName = "";
                                String organization = "";
                                String mail = "";
                                String phone = "";
                                String city = "";
                                String nickName = "";

                                // enumerate thru the card attributes.
                    for (Enumeration attEnum = attrSet.getEnumeration(); attEnum.hasMoreElements(); ) {
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

        public void     setValueAt(Object aValue, int row, int column) {
            ((Vector)mVecVec.elementAt(row)).setElementAt(aValue, column);
        }

        public void     reloadData (String aServerName, int aPort, String aSearchString) {
            //reload the data from LDAP.
            mVecVec = queryLDAP (aServerName, aPort, aSearchString);
        }

        public Object getValueAt(int row, int column) {
            return (((Vector)mVecVec.elementAt(row)).elementAt(column));
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
                setLayout(new BoxLayout(this, BoxLayout.X_AXIS));
        }

        public void paint(Graphics g) {
                super.paint(g);
        }
    }

    //***************************
    /**
     */
    class AddressPanel extends JPanel {

        public AddressPanel(DataSourceList aDataSourceList) {
                //super(true);

            setBorder (new EmptyBorder(10,10,10,10));
                setLayout (new BorderLayout(10, 5));

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
            for (Enumeration e = aDataSourceList.getEnumeration() ; e.hasMoreElements() ;) {
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
