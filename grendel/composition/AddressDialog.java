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

package grendel.composition;

import java.awt.*;
import java.awt.event.*;
import java.util.*;

import javax.swing.*;
import javax.swing.table.*;
import javax.swing.border.*;

class AddressDialog extends Dialog implements ActionListener {
    private final int BUTTON_WIDTH  = 100;
    private final int BUTTON_HEIGHT = 28;

    private List        mSendToList;    //ListBox gadget that holds recipients names.
    private JTable      mTable;         //Tbale displays the address book entries.
    private Vector      mAddresses;     //a parallel list of recipients with mSendToList.
    private boolean     wasCanceled = true; //true if Cancel button was used to dismiss dialog.

    //FIX remove
    final Object[][] data = {
        {"Name",
            "Lester Schueler",
            "Don Vale",
            "Chris Dreckman",
            "Fred Cutler",
            "Rod Spears"
        },
        {"Email Address",
            "lesters@netscape.com",
            "dvale@netscape.com",
            "chrisd@netscape.com",
            "fredc@netscape.com",
            "rods@netscape.com"
        },
        {"Organization",
            "Netscape Comm.",
            "Netscape Comm.",
            "Netscape Comm.",
            "Netscape Comm.",
            "Netscape Comm."
        },
        {"Phone",
            "(619)618-2227",
            "(619)618-2224",
            "(619)618-2211",
            "(619)618-2206",
            "(619)618-2228"
        }
    };

    //FIX remove
    public static void main(String[] args) {
        Frame frame = new JFrame();
        frame.setBackground(Color.lightGray);

        AddressDialog aDialog = new AddressDialog(frame);
        aDialog.show ();
        aDialog.dispose();

        frame.show();
    }

    /**
     *
     */
    AddressDialog(Frame aParent) {
        //FIX: Resource
        super(aParent, "Select Addresses", true);

        mAddresses = new Vector();

        setLayout(new BoxLayout(this, BoxLayout.Y_AXIS));

        JComponent lookupPanel = createLookupPanel ();
        add (lookupPanel);

        JComponent SendToPanel = createSendToPanel ();
        add (SendToPanel);

        JComponent bottomPanel = createBottomPanel ();
        add (bottomPanel);

        setResizable(false);
        setSize (716, 480);
    }

    /**
     *
     */
    private JPanel createLookupPanel () {
        //the outer most panel has groove etched into it.
        JPanel outerPanel = new JPanel(true);

        //FIX: Resource
        outerPanel.setBorder(new TitledBorder(new EtchedBorder(), "Type in the name you are looking for:"));
        outerPanel.setLayout (new BorderLayout ());
        outerPanel.setPreferredSize(new Dimension (500, 225));

            // "North" panel
            JPanel northPanel = new JPanel (true);
            northPanel.setLayout (new FlowLayout (FlowLayout.LEFT, 5, 5));
            northPanel.setPreferredSize(new Dimension (500, 35));

                //Search text field
                JTextField searchTxt = new JTextField (20);
                northPanel.add (searchTxt);

                //FIX: Resource
                northPanel.add (new JLabel ("In:"));

                JComboBox combo = new JComboBox();
                combo.setPreferredSize(new Dimension (275, 25));
                northPanel.add (combo);

                //FIX: Resource
                JButton b = makeButton ("Search", BUTTON_WIDTH, BUTTON_HEIGHT);
                ButtonModel bm = b.getModel();
                bm.setEnabled(false);
                northPanel.add (b);

            // Center table
            TableModel dataModel;

            // Create the table data model
            dataModel = new AbstractTableModel() {
                public int getRowCount() { return data[0].length - 1; }

                public int getColumnCount() { return data.length; }

                public Object getValueAt(int rowIndex, int columnIndex) {
                    return data[columnIndex][rowIndex+1];
                }

                public void setValueAt(Object aValue, int rowIndex, int columnIndex) {
                    data[columnIndex][rowIndex+1] = aValue;
                }

                public int getColumnIndex(Object aID) {
                  for (int i = 0; i < data.length; i++) {
                    if (data[i][0].equals(aID)) {
                      return i;
                    }
                  }
                  return -1;
                }
            };

            // Create the table
            mTable = new JTable(dataModel);

            // Added our columns into the  column model
            for (int columnIndex = 0; columnIndex < data.length; columnIndex++){
                // Create a column object for each column of data
                TableColumn newColumn = new TableColumn(columnIndex);

                newColumn.setWidth(200);
                mTable.addColumn(newColumn);
            }

            mTable.setColumnSelectionAllowed(false);
            mTable.setShowGrid(false);

            // Put the table and header into a scrollPane
            JScrollPane scrollpane = new JScrollPane();
            JTableHeader tableHeader = mTable.getTableHeader();

            // create and add the column heading to the scrollpane's
            // column header viewport
            JViewport headerViewport = new JViewport();
            headerViewport.setLayout(new BoxLayout(headerViewport, BoxLayout.X_AXIS));
            headerViewport.add(tableHeader);
            scrollpane.setColumnHeader(headerViewport);

            // add the table to the viewport
            JViewport mainViewPort = scrollpane.getViewport();
            mainViewPort.add(mTable);

            // speed up resizing repaints by turning off live cell updates
            tableHeader.setUpdateTableInRealTime(false);

            // South panel
            JPanel southPanel = new JPanel (true);

                //FIX: Resource
                southPanel.setLayout (new FlowLayout (FlowLayout.LEFT, 10, 0));
                southPanel.add (makeButton ("To:", BUTTON_WIDTH, BUTTON_HEIGHT));
                southPanel.add (makeButton ("Cc:", BUTTON_WIDTH, BUTTON_HEIGHT));
                southPanel.add (makeButton ("Bcc:", BUTTON_WIDTH, BUTTON_HEIGHT));

                //FIX
                JButton jb = makeButton ("Add to Address Book", 184, BUTTON_HEIGHT);
                ButtonModel jbm = jb.getModel();
                jbm.setEnabled(false);
                southPanel.add (jb);

        JPanel panel2 = new JPanel (new BorderLayout (5, 5), true);
        panel2.setBorder (new EmptyBorder (5, 5, 5, 5));
        panel2.add (northPanel, BorderLayout.NORTH);
        panel2.add (scrollpane, BorderLayout.CENTER);
        panel2.add (southPanel, BorderLayout.SOUTH);

        outerPanel.add (panel2, BorderLayout.CENTER);
        return outerPanel;
    }

    /**
     * The Send To Panel holds the list people.
     */
    private JPanel createSendToPanel () {
        //the outer most panel has groove etched into it.
        JPanel etchedPanel = new JPanel();

        //FIX: Resource
        etchedPanel.setBorder(new TitledBorder(new EtchedBorder(), "This message will be sent to:"));
        etchedPanel.setLayout (new BorderLayout (5, 5));
        etchedPanel.setPreferredSize(new Dimension (500, 175));

        JPanel panel2 = new JPanel (new BorderLayout (5, 5), true);
        panel2.setBorder (new EmptyBorder(5, 5, 5, 5));

        // Center Listbox panel
        mSendToList = new List();
        mSendToList.setBackground (Color.white);
        panel2.add (mSendToList, BorderLayout.CENTER);

        JPanel panel3 = new JPanel (new BorderLayout(), true);

        //FIX: Resource
        panel3.add (makeButton ("Remove", BUTTON_WIDTH, BUTTON_HEIGHT), BorderLayout.WEST);
        panel2.add (panel3, BorderLayout.SOUTH);

        etchedPanel.add (panel2, BorderLayout.CENTER);
        return etchedPanel;
    }

    /**
     * The bottom panel has the ok, cancel and help buttons.
     */
    private JPanel createBottomPanel () {
        JPanel panel = new JPanel (true);
        panel.setLayout (new FlowLayout (FlowLayout.CENTER, 25, 8));

        //FIX: Resource
        panel.add (makeButton ("OK", BUTTON_WIDTH, BUTTON_HEIGHT));
        panel.add (makeButton ("Cancel", BUTTON_WIDTH, BUTTON_HEIGHT));

        //FIX
        JButton b = makeButton ("Help", BUTTON_WIDTH, BUTTON_HEIGHT);
        panel.add (b);
        ButtonModel bm = b.getModel();
        bm.setEnabled(false);

        return panel;
    }

    private JButton makeButton (String aLabel, int aWidth, int aHeight) {
        JButton button = new JButton (aLabel);
        button.setPreferredSize(new Dimension (aWidth, aHeight));
        button.addActionListener (this);
        return button;
    }

    public Insets getInsets () {
        return (new Insets (25, 10, 10, 10));
    }

    /**
     * Adds an entry to the send list.
     * @param The Addressee to be added to the list.
     */
    private void addToSendList (Addressee aAddressee) {
        if (null != aAddressee) {
            mSendToList.add (aAddressee.deliveryString () + " " + aAddressee.getText());
            mAddresses.addElement (aAddressee);
        }
    }

    /**
     * Removes the specified entry from the list.
     * @param The index of the entry to remove.
     */
    private void removeFromSendList (int aIndex) {
        if (-1 < aIndex) {
            mSendToList.remove (aIndex);
            mAddresses.removeElementAt (aIndex);
        }
    }

    /**
     * Sets the address from aa Array.
     * @param aAddresses An array of addresses.
     * @see getAddresses
     */
    public void setAddresses (Addressee[] aAddresses) {
        mAddresses.removeAllElements();
        mSendToList.removeAll();

        if (null != aAddresses) {
            for (int i = 0; i < aAddresses.length; i++) {
                addToSendList (aAddresses[i]);
            }
        }
    }

    /**
     * Returns the addresses in the form of an array.
     * @returns An array of addresses.
     * @see setAddresses
     */
    public Addressee[] getAddresses () {
        int arrSize = mAddresses.size();

        if (0 < arrSize) {
            Addressee[] anArray = new Addressee[arrSize];
            mAddresses.copyInto(anArray);
            return anArray;
        }

        return null;
    }

   /**
     * Return true if cancel was pressed.
     */
    public boolean getCanceled () { return wasCanceled; }

   /**
     * Actions to respond to are button pressed for "To", "Cc", "Bcc", "Ok", and "Canel"
     */
    public void actionPerformed (ActionEvent e) {
        String s = e.getActionCommand ();

        //FIX: Resource
        if (s.equals ("To:")) {
            int selectedRow = mTable.getSelectedRow();
            if (-1 != selectedRow)
                addToSendList (new Addressee ((String) data[1][selectedRow + 1], Addressee.TO));
        }
        //FIX: Resource
        else if (s.equals ("Cc:")) {
            int selectedRow = mTable.getSelectedRow();
            if (-1 != selectedRow)
                addToSendList (new Addressee ((String) data[1][selectedRow + 1], Addressee.CC));
        }
        //FIX: Resource
        else if (s.equals ("Bcc:")) {
            int selectedRow = mTable.getSelectedRow();
            if (-1 != selectedRow)
                addToSendList (new Addressee ((String) data[1][selectedRow + 1], Addressee.BCC));
        }
        //FIX: Resource
        else if (s.equals ("Remove")) {
            int index = mSendToList.getSelectedIndex();

            if (-1 < index)
                removeFromSendList (index);
        }
        //FIX: Resource
        else if (s.equals ("Cancel")) {
            wasCanceled = true;
            setVisible(false);
        }
        //FIX: Resource
        else if (s.equals ("OK")) {
            wasCanceled = false;
            setVisible(false);
        }
    }
}
