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

import java.io.Serializable;
import java.awt.*;
import javax.swing.*;
import javax.swing.border.*;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

public class OptionsPanel extends JPanel implements Serializable  {
  // private final int BOX_WIDTH = 300;
    private final int BOX_WIDTH = 160;
    private final int BOX_HEIGHT = 30;

    private GridBagConstraints c;
    private GridBagLayout gridbag;

    private LabeledCombo ident;

    public OptionsPanel () {
        super ();
        
        c = new GridBagConstraints();
        gridbag = new GridBagLayout();
        
        setLayout(gridbag);

        c.fill = GridBagConstraints.HORIZONTAL;
        c.weightx = 1.0;
        c.gridheight = 1;

        addCheck ("Return Receipt", "Return Receipt", 'r', false);

        c.gridwidth = GridBagConstraints.REMAINDER; //end row

        LabeledCombo priority = new LabeledCombo("Priority");
        priority.addPossibleValue("Lowest");
        priority.addPossibleValue("Low");
        priority.addPossibleValue("Normal");
        priority.addPossibleValue("High");
        priority.addPossibleValue("Highest");
        addToGridBag(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, priority), gridbag, c);

        c.gridwidth = 1;

        addCheck ("Uuencoded instead of MIME for attachments", "UUEncode", 'u', false);

        c.gridwidth = GridBagConstraints.REMAINDER; //end row

        LabeledCombo format = new LabeledCombo("Format");
        format.addPossibleValue("Ask me");
        format.addPossibleValue("Plain Text only");
        format.addPossibleValue("HTML Text only");
        format.addPossibleValue("Plain Text and HTML");
        addToGridBag(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, format), gridbag, c);

        c.gridwidth = 1;

        c.gridwidth = GridBagConstraints.REMAINDER; //end row

        ident = new LabeledCombo("Identity");

	// Read all the different identities from the preferences file
        Preferences prefs = PreferencesFactory.Get();
        int numIdentities = prefs.getInt("mail.identities", 1);
        for (int i=0; i<numIdentities; i++) {
            ident.addPossibleValue(prefs.getString("mail.identity-" + i + ".description", "(nobody)"));
        }
        // Select the first (default) identity (number 0)
        ident.setSelectedIndex(0);
        
        addToGridBag(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, ident), gridbag, c);

        c.gridwidth = GridBagConstraints.REMAINDER; //end row

        addCheck ("Signed", "Signed", 'd', false);

    }

    public int getSelectedIdentity() {
    	return ident.getSelectedIndex();
    }
    
    private void addToGridBag (Component aComponent, GridBagLayout gridbag, GridBagConstraints c) {
    	gridbag.setConstraints(aComponent, c);
    	add(aComponent);
    }

    private void addCheck (String aLabel, String aToolTip) {
        addCheck (aLabel, aToolTip, ' ', false);
    }

    private void addCheck (String aLabel, String aToolTip, boolean isChecked) {
        addCheck (aLabel, aToolTip, ' ', isChecked);
    }

    private void addCheck (String aLabel, String aToolTip, char aAccelerator, boolean isChecked) {
                JCheckBox checkButton = new JCheckBox(aLabel, isChecked);

            if (!aToolTip.equals ("")) checkButton.setToolTipText(aToolTip);
        if (' ' != aAccelerator) checkButton.setMnemonic(aAccelerator);

        //create a fixed sized panel.
        addToGridBag(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, checkButton), gridbag, c);
    }

    class FixedSizedPanel extends JPanel {
        FixedSizedPanel (int aWidth, int aHeight, Component aComp) {
            setPreferredSize (new Dimension (aWidth, aHeight));
            setMaximumSize (getPreferredSize());
            //    setLayout (new BorderLayout ());
            setLayout (new GridLayout(1,1,5,5));
            setBorder (BorderFactory.createEmptyBorder (0, 5, 0, 0));
            //   add ("West", aComp);
            add (aComp);
        }
    }

    class LabeledCombo extends JPanel {
        JComboBox   mComboBox;
        JLabel      mLabel;

        public LabeledCombo (String aLabel) {

            mComboBox   = new JComboBox ();
            mLabel      = new JLabel (aLabel);

            mComboBox.setPreferredSize (new Dimension (150, 20));

            setLayout (new BorderLayout(5, 5));
//            setInsets (new Insets (5, 5, 5, 5));

            add ("West", mLabel);
            add ("Center", mComboBox);
        }

        public void addPossibleValue(String aValue) {
            mComboBox.insertItemAt (aValue, mComboBox.getItemCount());
        }
        
        public void setSelectedIndex(int anIndex) {
            mComboBox.setSelectedIndex(anIndex);
        }

        public int getSelectedIndex() {
            return mComboBox.getSelectedIndex();
        }
    }
}
