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

/* Gadget oganization:
    an AttachmentsList has one AttachmentsPanel.
        an AttachmentsPanel has many AttachmentLabel(s).
 */

import java.io.Serializable;
import java.util.*;
import java.awt.*;
import java.awt.event.*;

import javax.swing.*;

/*
*/
//public class AttachmentsList extends JScrollPane implements Serializable, FocusListener {
public class AttachmentsList extends JScrollPane implements Serializable {
    private AttachmentsPanel mAP;

    public AttachmentsList () {
        super ();

        //add attachments panel.
        mAP = new AttachmentsPanel ();
        JViewport spViewPort = getViewport();
        spViewPort.add(mAP);

        //keyboard navigation (arrows).
        addKeyListener (mAP);

//        mAP.addFocusListener (this);

        setBackground (Color.white);
    }

    /**
     * Sets this gadgets enabled state.
     * @param aEnabled Boolean value for enabled.
     */
    public void setEnabled (boolean aEnabled) {
    }

    /**
     * showDialog requests that the AddressList display its file dialog.
     * @see showDialog()
     */
    public void showDialog () {
        mAP.showDialog ();
    }

    /**
     * Delets all attachments from the list.
     * @see addAttachment
     * @see removeAttachment
     */
    public void removeAllAttachments () {
    }

    /**
     * Remove an entry at index from the list.
     * @param aIndex The index of the attachment to remove.
     * @see removeAllAttachments
     */
    public void removeAttachment (int aIndex) {
    }

    /**
     * Adds an attachment to the list.
     * @param aFilePath  A file path.
     * @see removeAllAttachments
     */
    public void addAttachment (String aFilePath) {
        mAP.addAttachment (aFilePath);
    }

    /**
     * Tranverse up a component's tree to find the top most frame.
     * @param aComp the component for which you want its parent frame for.
     * @return The parent frame.
     */
    public static Frame getTopLevelFrame(Component aComp) {
        Component c = aComp;

        while (null != c.getParent())
            c = c.getParent();

        if (c instanceof Frame)
            return (Frame) c;

        return null;
    }

    //*****************
    /**
     *
     */
    class AttachmentsPanel extends JPanel implements MouseListener, KeyListener {
        private AttachmentLabel mCurrentSelection;

        public AttachmentsPanel () {
            super ();

            //do your own layout.
            setLayout (null);

            //Get your own mouse events.
            //  used to diaply file dialog when mouse down on background.
            addMouseListener (this);

            setBackground (Color.white);
        }

        /**
         * Gets the currently selected label.
         * @return The currently selected label.
         * @see setSelection
         */
        private AttachmentLabel getSelection () { return mCurrentSelection; }

        /**
         * Sets the currently selected label.
         * @param One of the AttachmentLabel's
         * @see getSelection
         */
        private void setSelection (AttachmentLabel aLabel) {
            //un-hightlight old.
            if (null != mCurrentSelection) {
                mCurrentSelection.setForeground (Color.black);
                mCurrentSelection.setBackground (Color.white);
            }

            mCurrentSelection = aLabel;

            //highlight new
            if (null != mCurrentSelection) {
                mCurrentSelection.setForeground (Color.white);
                mCurrentSelection.setBackground (Color.blue);
            }

            repaint();
        }

        /**
         * Adds a file attachment to the list.
         * @param aFilePath  A file path.
         * @see AttachmentsList.addAttachment
         */
        protected void addAttachment (String aFilePath) {
            AttachmentLabel fileLabel = new AttachmentLabel (aFilePath);
            add (fileLabel);

            //mouse listener for selection via mouse (click on the line).
            fileLabel.addMouseListener (this);

            //select the newly added attachment.
            setSelection (fileLabel);
            doLayout();
        }

        /**
         * Remove a file attachment from the list.
         */
        private void removeAttachment (AttachmentLabel aAttLabel) {
            if (null != aAttLabel) {
                remove (aAttLabel);

                //if the AttachmentLabel that's being removed is also the currently
                //  selected label then set the current selection to null.
                if (aAttLabel == getSelection())
                    setSelection(null);

                //force a re-layout of the labels.
                doLayout();
            }
        }

        /**
         * Remove a file attachment from the list.
         */
        private void removeAttachment (int aIndex) {
        }

        /**
         *
         */
        public void doLayout() {
            Graphics    g   = getGraphics();
            FontMetrics fm  = g.getFontMetrics();
            Dimension   size = getSize();
            int         y, i;
            Component   comps[] = getComponents();
            int         height = new Double(fm.getHeight() * 1.2).intValue();

            for (   i = 0, y = 0;
                    (i < comps.length)  && (y < size.height);
                    i++, y += fm.getHeight()) {

                AttachmentLabel attLabel = (AttachmentLabel) comps[i];
                attLabel.setBounds(3, y, size.width, height);
            }
        }

        public Dimension getMaximumSize() {
                return getPreferredSize();
        }

        public Dimension getMinimumSize() {
                return getPreferredSize();
        }

        public Dimension getPreferredSize() {
            Graphics    g   = getGraphics();
            FontMetrics fm  = g.getFontMetrics();
            Component   comps[] = getComponents();
            int         totalHeight = new Double((comps.length + 1) * fm.getHeight() * 1.2).intValue();

                return new Dimension(100, totalHeight);
        }

        /**
         * Display the file dialog so the user can select a file to add to the list.
         * @see this.mousePressed and AttachmentsList.showDialog()
         */
        protected void showDialog () {
            //get the parent frame for the file dialog.
            Frame parentFrame = getTopLevelFrame(this);

            if (null != parentFrame) {
                FileDialog file = new FileDialog (parentFrame, "Attach File", FileDialog.LOAD);

                file.setFile ("*.*");   //all files.
                file.show();            //show the dialog. This blocks.

                //didi they select a file?
                if (null != file.getFile()) {

                    //assemble the full path.
                    String fullPath = file.getDirectory() + file.getFile();
                    addAttachment (fullPath);
                }
            }
        }

        /**
         *
         */
        public void mouseEntered (MouseEvent e) {}
        public void mouseExited  (MouseEvent e) {}
        public void mouseReleased(MouseEvent e) {}
        public void mouseClicked (MouseEvent e) {}

        //on any mouse click show the popup menu.
        public void mousePressed (MouseEvent e) {
            Component comp = e.getComponent();

            //mouse down on background (this panel) so dispaly file dialog.
            if (comp == this) {
                showDialog ();
            }

            //else mouse down on an AttachmentLabel so select line.
            else if (comp instanceof AttachmentLabel) {
                setSelection ((AttachmentLabel)e.getComponent());
            }
        }

        /**
         * keyPressed responds to up and down arrows keys.
         */
        //implements KeyListener...
        public void keyReleased (KeyEvent e) {}
        public void keyTyped    (KeyEvent e) {}

        public void keyPressed  (KeyEvent e) {
            if (KeyEvent.KEY_PRESSED == e.getID()) {

                //delete the selected attachment.
                if (KeyEvent.VK_DELETE == e.getKeyCode()) {
                    AttachmentLabel current = getSelection();

                    if (null != current)
                        removeAttachment (current);
                }

                //else arrow up or down (change selection)
                else if ((KeyEvent.VK_UP   == e.getKeyCode()) ||
                         (KeyEvent.VK_DOWN == e.getKeyCode()))
                {
                    //get the currently selected attachment and all attachments.
                    AttachmentLabel current = getSelection();
                    Component comps[] = getComponents();
                    int i = 0;

                    //find the index of the current attachment
                    for (i = 0; i < comps.length; i++) {
                        if (current == (AttachmentLabel) comps[i])
                            break;
                    }

                    //leave if not found.
                    if (i == comps.length)
                        return;

                    //increment/decrement
                    if (KeyEvent.VK_UP == e.getKeyCode())
                        i += comps.length -1;
                    else    //(KeyEvent.VK_DOWN == e.getKeyCode())
                        i += comps.length +1;

                    //select next and wrap using the modulus.
                    setSelection ((AttachmentLabel) comps[i % comps.length]);
                }
            }
        }
    }

    //*****************
    /** An AttachmentLabel displays the file name.
     */
    class AttachmentLabel extends JLabel {
        AttachmentLabel (String aLabel) {
            super (aLabel);
        }

        /**
        * Processes all Mouse events
        */
        public void processMouseEvent(MouseEvent e) {
           switch(e.getID()) {
             case MouseEvent.MOUSE_PRESSED:
              requestFocus();
              break;

             case MouseEvent.MOUSE_RELEASED:
                break;
             case MouseEvent.MOUSE_EXITED:
                break;
           }
           super.processMouseEvent(e);
        }

          public void paint (Graphics g) {
            Dimension size = getSize();
            FontMetrics fm = g.getFontMetrics();

            g.setColor (getBackground());
            g.fillRect (0, 0, size.width, size.height);

            g.setColor (getForeground());
            g.drawString (getText(), 0, fm.getHeight());
        }
    }
}

