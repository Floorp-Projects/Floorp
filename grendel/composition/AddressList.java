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
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.composition;

/* Gadget organization:
    an AddressList has one AddressPanel.
        an AddressPanel has many AddressLine(s).
            an AddressLine has one DeliveryButton, one AddressIcon and one AddressTextField.
 */

import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.util.*;

import javax.swing.*;
import javax.swing.JTable;

//import netscape.orion.misc.*;

public class AddressList extends JScrollPane implements Serializable {
    protected AddressPanel mAddressPanel;

    public AddressList() {
        super();

        //scroll panel
        JViewport spViewPort = getViewport();

        //create addressList panel
        mAddressPanel = new AddressPanel ();

        //add address list panel to scroll panel.
        spViewPort.add(mAddressPanel);

        setBackground (Color.white);
    }

    int mProp1 = 0;
    public void setProp1 (int aVal) {
        mProp1 = aVal;
    }

    public int getProp1 () {
        return mProp1;
    }

    int mProp2 = 0;
    public void setProp2 (int aVal) {
        mProp2 = aVal;
    }

    public int getProp2 () {
        return mProp2;
    }

    /**
     * Clears and sets the addresses from an Array.
     * @param aAddresses An array of addresses.
     * @see getAddresses()
     */
    public void setAddresses (Addressee[] aAddresses) {
        mAddressPanel.removeAllAddressLines (); //delete all entries.

        if (null != aAddresses) {
            for (int i = 0; i < aAddresses.length; i++) {

                if (null != aAddresses[i])
                    mAddressPanel.addAddresseLine (aAddresses[i]);
            }
        }

        mAddressPanel.repack();
    }

    /**
     */
    public Dimension getPreferredSize() {
                return mAddressPanel.getPreferredSize();
    }

    /**
     * Returns the addresses in the form of an array.
     * @returns An array of addresses.
     * @see setAddresses()
     */
    public Addressee[] getAddresses () {
        Vector temp = mAddressPanel.mAddressees;
        int i;

        //reject blank entries.
        for (i = 0; i < temp.size(); i++) {
            AddressLine al = (AddressLine) temp.elementAt(i);

            if (al.isBlank()) {
                temp.removeElement (al);
            }
        }

        //copy over from vector into array.
        Addressee[] anArray = new Addressee[temp.size()];
        for (i = 0; i < temp.size(); i++) {
            anArray[i] = ((AddressLine) temp.elementAt(i)).getAddressee();
        }

        return anArray;
    }

    /**
     *
     */
    public class AddressPanel extends JPanel implements KeyListener {
        protected Vector    mAddressees;    //Addresses
        private Dimension   mAddLineSize;   //The size of one AddressLine. For layout.
        private Dimension   mPerfSize;      //The preferred size of this panel = 4 * mAddLineSize.

        public AddressPanel () {
            super();
            setLayout (null);

            //vector for holding addressee list.
            mAddressees = new Vector();

            //create the first addressee to start them off.
            repack();
        }

        /**
         * layout Addressee lines.
         */
        public void doLayout() {
            int i;
            Enumeration e;
            Dimension size = getSize();

            /*place each addressee line in one column multiple rows.
            +---------------+
            | addressee #1  |
            |---------------|
            | addressee #2  |
            |---------------|
            | ...           |
            |---------------|
            | addressee #n  |
            +---------------+
            */
            for (e = mAddressees.elements(), i = 0; e.hasMoreElements(); i++) {
                AddressLine al = (AddressLine) e.nextElement();
                al.setBounds(0, i * mAddLineSize.height, size.width, mAddLineSize.height);
            }
        }

//        public Dimension getMaximumSize() {
//              return getPreferredSize();
//      }

//        public Dimension getMinimumSize() {
//              return getPreferredSize();
//      }

        public void addNotify () {
            super.addNotify ();

            //store the first AddressLine's size for layout.
            AddressLine al = (AddressLine) mAddressees.elementAt(0);
                mAddLineSize = al.getPreferredSize();

            //the preferred size of the panel is 4 AddressLines tall and full parent width.
            mPerfSize = new Dimension (mAddLineSize.width, 5 * mAddLineSize.height);
        }

        /**
         * Returns the preferred size.
         * The preferred size of the panel is 4 AddressLines tall and full parent width.
         * @return
         */
        public Dimension getPreferredSize() {
            //the preferred size of the panel is 4 AddressLines tall and full parent width.
            return (mPerfSize);
        }

        /**
         * Adds a new address line to the list.
         * @param aAddressee the Addresee you which to add.
         * @see removeAddressLine
         */
        private synchronized void addAddresseLine (Addressee aAddressee) {
            //create the new AddressLine.
             AddressLine al = new AddressLine (aAddressee);

            //Add a keyboard listener for navigation
            //so they may navigate BETWEEN AddressLines using arrow keys.
            al.atfAddKeyListener (this);

            add (al);                           //add AddressLine gadget to panel
            mAddressees.addElement (al);        //remember in vector table
            validate();                         //layout control again (scrollbar may appear).
        }

        /**
         * Remove any blank lines from the middle
         *  and appends a single blank line to the list.
         */
        private void repack () {
            //remove blank lines
            for (int i = 0; i <  mAddressees.size() - 2; i++) {
                AddressLine al = (AddressLine) mAddressees.elementAt(i);
                if (al.isBlank()) {
                    removeAddressLine(al);
                }
            }

            //no entries at all so add a blank line.
            if (0 == mAddressees.size()) {
                addAddresseLine (new Addressee ("", Addressee.TO));
            }
            else {
                //add a blank last line if needed.
                AddressLine lastAL = (AddressLine) mAddressees.elementAt(mAddressees.size() - 1);
                if (!lastAL.isBlank()) {
                    addAddresseLine (new Addressee ("", lastAL.getDeliveryMode()));
                }
            }
        }

        /*
         * Place focus on the last addressLine.
         */
        private void focusOnLast () {
            int last = mAddressees.size() - 1;

            //get its TextFiled and set focus on it.
            if (last > -1) {
                AddressLine al = (AddressLine) mAddressees.elementAt(last);
                al.atfRequestFocus();
            }
        }

        /**
         * Removes an address line from the list.
         * @param aAddressLine The address line you wish to remove.
         * @see addAddresseLine
         */
        private synchronized void removeAddressLine (AddressLine aAddressLine) {
            remove (aAddressLine);                      //remove AddressLine gadget from panel.

            aAddressLine.atfRemoveKeyListener (this);   //stop listening for key events.

            mAddressees.removeElement (aAddressLine);   //remove from vector table.
            validate();                                 //layout control again (scrollbar).
            repaint();
        }

        /**
         * Removes all addressee lines.
         * @see removeAddressLine
         * @see addAddresseLine
         */
        private synchronized void removeAllAddressLines () {
            removeAll();    //remove all AddressLines gadget from panel.
            mAddressees.removeAllElements();   //clear vector table.

            validate();     //layout control again (scrollbar).
            repaint();
        }

        /**
         * Resonds to keyboard events for navigation (up, down, enter, etc.)
         * @see addAddresseLine
         */
        //implements KeyListener...
        public void keyReleased (KeyEvent e) {}
        public void keyTyped (KeyEvent e) {}

        /* Recieve keyPressed events for this->AddressLine->AddressTextField.
            KeyEvent response table
            KEY_PRESSED     Old Position    State       New Positon     Behavior
            ---------------------------------------------------------------------------------------
            VK_UP           NOT first       -           Previous
            VK_UP           First           -           First
            VK_DOWN         NOT last        -           Next
            VK_DOWN         Last            -           Last
            VK_BACK_SPACE   First           line empty  First
            VK_BACK_SPACE   NOT first       line empty  Previous        Delete this address line.
            VK_DELETE       Last            line empty  Last
            VK_DELETE       NOT last        line empty  Same            Delete this address line.
        */
        public void keyPressed (KeyEvent e) {

            //filter out only the keys we're interested in.
            if ((KeyEvent.KEY_PRESSED == e.getID()) &&
                (   (KeyEvent.VK_UP         == e.getKeyCode())  ||
                    (KeyEvent.VK_BACK_SPACE == e.getKeyCode())  ||
                    (KeyEvent.VK_DELETE     == e.getKeyCode())  ||
                    (KeyEvent.VK_DOWN       == e.getKeyCode())
                )
               ) {

                //find out which AddressLine created this key press.
                Component sourceComp = e.getComponent();

                //check that this is an AddressTextField.
                if (sourceComp instanceof AddressTextField) {
                    AddressTextField sourceATF = (AddressTextField)sourceComp;

                    //get its parent AddressLine gadget.
                    AddressLine sourceAL = (AddressLine) sourceATF.getParent();

                    //locate the AddressLine in the vector table mAddressees.
                    int lastIndex = mAddressees.lastIndexOf(sourceAL);

                    // This should never happen.
                    //  (i.e. You should never get a KeyEvent from a gadget not in the mAddressees Vector)
                    // FIX: do something.
                    if (-1 == lastIndex) {
                        //System.out.println ("Internal error.");
                    }
                    else {

                        //calculate where to send focus depending on the keypress.
                        int incFocus = 0;    //increment focus (-1, 0, +1)

                        //UP
                        if (KeyEvent.VK_UP == e.getKeyCode()) {
                            incFocus = -1;  //focus previous.
                        }

                        //DOWN
                        else if (KeyEvent.VK_DOWN == e.getKeyCode()) {
                            incFocus = +1;  //focus next.
                        }

                        //BACKSPACE or DELETE
                        else if ((KeyEvent.VK_BACK_SPACE == e.getKeyCode()) ||
                                 (KeyEvent.VK_DELETE == e.getKeyCode())) {
                            //if they've pressed backspace or delete and this address line
                            //  is already empty them delete this AddressLine.

                            //no matter what, if there is only one AddressLine left
                            //  then don't delete it or try to change focus.
                            if (1 >= mAddressees.size())
                                return;

                            //Check to see if the current AddressTextField (ATF) is empty.
                            // If so then delete it.
                            if (sourceAL.isBlank()) {

                                //remove the AddressLine from this container and vector list.
                                removeAddressLine (sourceAL);

                                if (KeyEvent.VK_BACK_SPACE == e.getKeyCode())
                                    incFocus = -1;  //focus previous.
                                else //KeyEvent.VK_DELETE == e.getKeyCode()
                                    incFocus = 0;  //stay where you are.
                            }
                        }

                        //set focus to the next AddressTextField.
                        {
                            int nextIndex = lastIndex + incFocus;

                            if (nextIndex < 0) {
                                nextIndex = 0;
                            }
                            else if (nextIndex > (mAddressees.size() - 1)) {
                                nextIndex = mAddressees.size() - 1;
                            }

                            //get its TextFiled and set focus on it.
                            AddressLine nextAL = (AddressLine) mAddressees.elementAt(nextIndex);
                            nextAL.atfRequestFocus();
                        }
                    }
                }
            }
        }

        public void paint(Graphics g) {
             //paint the AddressLine gadets.
            super.paint(g);

            Dimension size = getSize();

            g.setColor (Color.blue);

            //draw horizonttal lines BELOW the AddressLine gadgets.
            for (int i = mAddressees.size() * mAddLineSize.height; i < size.height; i += mAddLineSize.height) {
                g.drawLine (0, i, size.width, i);
            }

            //draw Vertical line BELOW the AddressLine gadgets and lined up with the left side addressee button.
            if (0 < mAddressees.size()) {
                AddressLine firstAddressLine = (AddressLine) mAddressees.elementAt(0);
                int buttonWidth = firstAddressLine.getButtonWidth();
                g.drawLine (buttonWidth, mAddressees.size() * mAddLineSize.height, buttonWidth, size.height);
            }
        }
    }

    //*************************
    /**
     * An AddressLine has one DeliveryButton and one AddressTextField.
     */
    public class AddressLine extends JPanel {
        private DeliveryButton      mDeliveryButton;
        private AddressTextField    mAddressTextField;
        private DragIcon            mDragIcon;
//        private boolean             mShowDeliveryButton = true;

        public AddressLine (Addressee aAddressee) {
            super();
            setLayout (null);   //see doLayout.

            //left side delivery button ("To:")
            mDeliveryButton = new DeliveryButton (aAddressee.getDelivery());
            add (mDeliveryButton);

            //center DragIcon
            mDragIcon = new DragIcon();
            add (mDragIcon);

            //right side text field ("john_doe@company.com")
            mAddressTextField = new AddressTextField (aAddressee.getText(), mDeliveryButton);
            add (mAddressTextField);
        }

        protected   void    atfRemoveKeyListener(KeyListener kl){ mAddressTextField.removeKeyListener (kl); }
        protected   void    atfAddKeyListener(KeyListener kl)   { mAddressTextField.addKeyListener (kl); }
        protected   void    atfRequestFocus()                   { mAddressTextField.requestFocus(); }
        protected   int     getButtonWidth()                    { return mDeliveryButton.getPreferredSize ().width; }
        protected   boolean isBlank()                           { return mAddressTextField.getText().equals (""); }
        protected   int     getDeliveryMode()                   { return mDeliveryButton.getDeliveryMode (); }

        /**
         * layout Delivery Button, DragIcon and AddressTextField.
         */
        public void doLayout() {
            /* Layout
                +----------------+----------+---------------------+
                | DeliveryButton | DragIcon | AddressTextField >>>|
                +----------------+----------+---------------------+
            */
            Dimension mySize = getSize(); //get this containers size.

            //DeliveryButton
            Dimension dbSize = mDeliveryButton.getPreferredSize ();
            mDeliveryButton.setBounds(0, 0, dbSize.width, mySize.height);
            int x = dbSize.width;

            //DragIcon
            Dimension diSize = mDragIcon.getPreferredSize ();
            mDragIcon.setBounds(x, 0, diSize.width, mySize.height);
            x += diSize.width;

            //AddressTetField takes up the rest.
            Dimension atfSize = mAddressTextField.getPreferredSize ();
            mAddressTextField.setBounds(x, (mySize.height - atfSize.height)/2, mySize.width - x, mySize.height);
        }

        /*
         */
            public Dimension getPreferredSize() {
            return mDeliveryButton.getPreferredSize ();
            }

        /*
         * Returns an Addressee for the line.
         */
        protected Addressee getAddressee() {
            return (new Addressee (mAddressTextField.getText(), mDeliveryButton.getDeliveryMode ()));
        }

        /**
         * Paint the blue lines in the background.
         */
        public void paint(Graphics g) {
            super.paint(g);

            Dimension size = getSize();
            FontMetrics fm = g.getFontMetrics();
            int buttonWidth = getButtonWidth();

            g.setColor (Color.blue);
            g.drawLine (buttonWidth, 0, size.width, 0);   //top
            g.drawLine (buttonWidth, size.height, size.width, size.height);   //bottom
        }
    }

    //*************************
    /**
     * Image icon for drag and drop.
     */
    public class DragIcon extends JPanel {
        private ImageIcon           mIcon;

        public DragIcon () {
            super();

            setBorder(BorderFactory.createEmptyBorder (5, 8, 5, 8));

            //create image icon for drag and drop.

                mIcon = new ImageIcon(getClass().getResource("images/card.gif"));
        }

        public void paint (Graphics g) {
            Dimension size = getSize();

            //try to center the icon.
            int x = (size.width - mIcon.getIconWidth())/2;
            x = (x < 0) ? 0 : x;
            int y = (size.height - mIcon.getIconHeight())/2;
            y = (y < 0) ? 0 : y;

            mIcon.paintIcon (this, g, x, y);
        }
    }

    public class AddressTextField extends JTextField implements FocusListener {
  //    public class AddressTextField extends ATC_Field implements FocusListener {
        private final String ADDRESS_SEPERATORS = ",";
        private final String ADDRESS_QUOTES = "\"";

        private DeliveryButton mDeliveryButton;

        public AddressTextField (String aString, DeliveryButton aDeliveryButton) {
          super(aString);
          //    super (aString, new TestDataSource2());
            mDeliveryButton = aDeliveryButton;

            //red completion text.
            //         setCompletionColor (Color.red);

            //NO border.
            setBorder(null);

            //get focus gained/lost events.
            addFocusListener(this);

            //catch tabs and enters before anyone else.
            enableEvents (AWTEvent.KEY_EVENT_MASK); //see processKeyEvent
        }

      public void setCompletionColor(Color c) {
        
      }

            /**
         * catch tabs before anyone else.
         */
         public void processKeyEvent (KeyEvent e) {
            //TAB
            if ('\t' == e.getKeyChar())
                return; //ignore tab characters.

            //ENTER
            if ('\n' == e.getKeyChar()) {
                evaluate ();                    //evaluate this line for mutilple entries.
                mAddressPanel.focusOnLast();    //Put focus on the last blank entry.
            }

            super.processKeyEvent(e);
         }

            /**
             * stub
         */
        public void focusGained(FocusEvent evt) {
        }

            /**
             * On focusLost, evaluate line for multiple entries.
         */
        public void focusLost(FocusEvent evt) {
            evaluate ();
        }

            /**
             * Evaluate line for multiple addresses, notify parent to add the new entries.
         */
        private void evaluate() {
            String [] tokens = parseLine (getText());

            //we've lost focus and they type nothing or a bunch of ADDRESS_SEPERATORS on this line.
            if (tokens.length == 0) {
                setText ("");
            }

            //else they typed something....
            else {
                //we keep the first.
                setText (tokens[0]);

                //if more than one address is on this line then add the others.
                if (tokens.length > 1) {
                    for (int i = 1; i < tokens.length; i++) {
                        mAddressPanel.addAddresseLine (new Addressee (tokens[i], mDeliveryButton.getDeliveryMode()));
                    }
                }
            }

            //repack the lines. (remove blanks)
            mAddressPanel.repack();
        }

            /**
             * Parses up the string.
             * @param aString The String to parse.
             * @return returns an array of strings.
             * @see ADDRESS_SEPERATORS
             * @see ADDRESS_QUOTES
         */
        private String[] parseLine (String aString) {
            Vector tokenVec = new Vector ();
            boolean quoted = false;
            int tail = 0;
            int head = 0;

            //step through each character in the string.
            for (head = 0; head < aString.length(); head++) {

                //is this a quote character?
                if (-1 != ADDRESS_QUOTES.indexOf(aString.charAt(head))) {

                    //are we already in a quoted string?
                    if (quoted) {
                        String token = aString.substring(tail, head + 1).trim();

                        //if this is not a blank then increment count.
                        if (!token.equals(""))
                            tokenVec.addElement(token);

                        tail = head + 1;
                        quoted = false; //quoting off
                    }
                    else {
                        tail = head;    //remember the quote char.
                        quoted = true;   //quoting on.
                    }
                }

                //is this a seperator character?
                else if (-1 != ADDRESS_SEPERATORS.indexOf(aString.charAt(head))) {

                    //if not in a quote and not
                    if (!quoted) {
                        String token = aString.substring(tail, head).trim();

                        //if this is not a blank then increment count.
                        if (!token.equals(""))
                            tokenVec.addElement(token);

                        tail = head + 1;
                    }
                }
            }

            //last token
            String quotingChar = "";

            //did we finish with an open quote?
            if (quoted) {
                quotingChar = aString.substring(tail, tail + 1);   //add a matching quote.
            }

            String token = aString.substring(tail, head).trim();

            //if this is not a blank then increment count.
            if (!token.equals(""))
                tokenVec.addElement(token + quotingChar);

            //return an array of Strings.
            String [] tokenArray = new String [tokenVec.size()];
            tokenVec.copyInto(tokenArray);
            return tokenArray;
        }
    }

    //*************************
    /**
     * DeliveryButton displays "To:", "Cc:", etc and has a popup menu to change values.
     */
    public class DeliveryButton extends JPanel implements
                                            MouseListener,  //show popup menu on mouse click
                                            FocusListener,  //display focus when tabbed to.
                                            ActionListener, //get popup menu selection.
                                            KeyListener {   //show popup menu when focus and spacebar pressed.

        private int         mDeliveryMode;
        private Dimension   mPerfSize = null;
        private Insets      mInsets = new Insets (4, 4, 4, 8);
        private PopupMenu   mPopup;

        public DeliveryButton (int aDeliveryMode) {
            //get your own mouse events for popup menu.
            addMouseListener (this);

            //get your own focus events for button highlight.
            addFocusListener (this);

            //display popup menu when in focus and spacebar pressed.
            addKeyListener (this);

            //create the popup menu..
            mPopup = createPopup ();

            //add popup to me.
            add (mPopup);

            setDeliveryMode (aDeliveryMode);
        }

        /**
         * Creates the popup menu.
         */
        private PopupMenu createPopup () {
            PopupMenu pm = new PopupMenu (Addressee.getDeliveryTitle());

            //added text commands to popup
            for (int i = 0; i < Addressee.mDeliveryStr.length; i++) {
                MenuItem mi = new MenuItem (Addressee.mDeliveryStr[i]);
                pm.add (mi);
                mi.addActionListener (this);
            }

            return pm;
        }

        /*
         * Set the button delivery mode.
         * @param aDeliveryMode a value like Addressee.TO or Addressee.BCC
         * @see getDeliveryMode
        */
        protected void setDeliveryMode (int aDeliveryMode) {
            mDeliveryMode = aDeliveryMode;
            repaint();
        }

        /*
         * Return the button delivery mode.
         * @return a delivery mode a value like Addressee.TO or Addressee.BCC
         * @see setDeliveryMode
        */
        protected int getDeliveryMode () { return mDeliveryMode; }

            public Dimension getMaximumSize() {
                return getPreferredSize();
        }

            public Dimension getMinimumSize() {
                return getPreferredSize();
        }

        /**
         * addNotify creates the preferred size dimension because only
         * after the peer has been created can we get the font metrics.
         */
        public void addNotify () {
            super.addNotify ();

            if (null == mPerfSize) {
                Font fnt = getFont();
                if (null != fnt) {
                    FontMetrics fm = getToolkit().getFontMetrics(fnt);
                    if (null != fm) {
                        String longestString = Addressee.getLongestString();

                        mPerfSize = new Dimension(fm.stringWidth(longestString) + mInsets.left + mInsets.right,
                                                 fm.getMaxAscent() + fm.getMaxDescent() + mInsets.top + mInsets.bottom);
                    }
                }
            }
        }

        /**
         * Return PerfSize created in addNotify.
         */
            public Dimension getPreferredSize() {
            if (null != mPerfSize) {
                return mPerfSize;
            }
            return new Dimension(50, 20);
            }

        public boolean isFocusTraversable() {
            return true;
        }

            /**
             * Display button highlight.
         */
        public void focusGained(FocusEvent evt) {
            repaint();
        }

            /**
             * Remove button highlight.
         */
        public void focusLost(FocusEvent evt) {
            repaint();
        }

            /**
         * Paint the button's bevels and arrow and text string (i.e. "To:")
         */
        public void paint(Graphics g) {
            Dimension size = getSize();

            //gray background
            g.setColor (Color.lightGray);
            g.fillRect (0, 0, size.width - 1, size.height - 1);

            //draw bezel borders
            g.setColor (Color.white);
            g.drawLine (0, 0, 0, size.height - 1); //top
            g.drawLine (0, 0, size.width - 1, 0); //left
            g.setColor (Color.black);
            g.drawLine (size.width - 1, 0, size.width - 1, size.height - 1); //right
            g.drawLine (0, size.height - 1, size.width - 1, size.height - 1); //bottom

            //down arrow
            g.setColor (Color.gray);

            int xPoints[]  = {0, 10, 5};
            int yPoints[]  = {0, 0, 10};
            Polygon arrow = new Polygon (xPoints, yPoints, 3);
            Rectangle arrowRect = arrow.getBounds();

            int deltaX = mInsets.left;
            int deltaY = (size.height - arrowRect.height)/2;

            arrow.translate (deltaX, deltaY);

            g.fillPolygon(arrow);

            g.setColor (Color.white);
            g.drawLine (xPoints[1] + deltaX,
                        yPoints[1] + deltaY,
                        xPoints[2] + deltaX,
                        yPoints[2] + deltaY); //right edge

            //Delivery mode text (ie. "To:")
            g.setColor (Color.black);
            FontMetrics fm = g.getFontMetrics();

            //Font height is wacky.
            //g.drawString (mLabel, arrowRect.width + mInsets.left + 4, (fm.getHeight() + size.height)/2);
            g.drawString (Addressee.deliveryToString(mDeliveryMode), arrowRect.width + mInsets.left + 4, fm.getHeight());

            //draw focus rectangle (AWT 1.1 doesn't have support for line styles)
            if (true == hasFocus()) {
                final int offset = 3;   //insets of focus rectangle.

                //top and bottom
                int bottomOffset = size.height - offset - 1;
                for (int x = offset; x < size.width - offset; x += 2) {
                    g.drawLine (x, offset, x, offset);   //top
                    g.drawLine (x, bottomOffset, x, bottomOffset); //bottom
                }

                //left and right
                int rightOffset = size.width - offset - 1;
                for (int y = offset; y < size.height - offset; y += 2) {
                    g.drawLine (offset, y, offset, y);   //left
                    g.drawLine (rightOffset, y, rightOffset, y);  //right
                }
            }
        }

        //implements KeyListener...
        public void keyReleased (KeyEvent e) {}
        public void keyTyped (KeyEvent e) {}

        /*
         * Display popup menu when spacebar pressed.
        */
        public void keyPressed (KeyEvent e) {
            if ((KeyEvent.KEY_PRESSED == e.getID()) &&
                (KeyEvent.VK_SPACE == e.getKeyCode())) {
                mPopup.show (e.getComponent (), 0, 0);
            }
        }

        /**
         * Called when popup menu item is selected.
         */
        public void actionPerformed (ActionEvent e) {
            String menuCommand = e.getActionCommand();
            setDeliveryMode (Addressee.deliveryToInt (menuCommand));
        }

        public void mouseEntered (MouseEvent e) {}
        public void mouseExited  (MouseEvent e) {}
        public void mouseReleased(MouseEvent e) {}
        public void mouseClicked (MouseEvent e) {}

        /**
         * On any mouse click show the popup menu.
         * Windows has bug for getX(), getY(). (UNIX OK)
         */
        public void mousePressed (MouseEvent e) {
            mPopup.show (e.getComponent (), e.getX(), e.getY());
        }
    }
}
