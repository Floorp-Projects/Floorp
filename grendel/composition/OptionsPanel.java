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

public class OptionsPanel extends JPanel implements Serializable  {
    private final int BOX_WIDTH = 300;
    private final int BOX_HEIGHT = 30;

    public OptionsPanel () {
        super ();

        setLayout(new GridLayout(3, 2, 5, 5));

        addCheck ("Encrypted", "Encrypted", 'e', false);

        addCheck ("Return Receipt", "Return Receipt", 'r', false);

        addCheck ("Signed", "Signed", 'd', false);

        LabeledCombo priority = new LabeledCombo("Priority");
        priority.addPossibleValue("Lowest");
        priority.addPossibleValue("Low");
        priority.addPossibleValue("Normal");
        priority.addPossibleValue("High");
        priority.addPossibleValue("Highest");
        add(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, priority));

        addCheck ("Uuencoded instead of MIME for attachments", "", 'u', false);

        LabeledCombo format = new LabeledCombo("Format");
        format.addPossibleValue("Ask me");
        format.addPossibleValue("Plain Text only");
        format.addPossibleValue("HTML Text only");
        format.addPossibleValue("Plain Text and HTML");
        add(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, format));
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
        add(new FixedSizedPanel (BOX_WIDTH, BOX_HEIGHT, checkButton));
    }

    class FixedSizedPanel extends JPanel {
        FixedSizedPanel (int aWidth, int aHeight, Component aComp) {
            setPreferredSize (new Dimension (aWidth, aHeight));
            setLayout (new BorderLayout ());
            setBorder (BorderFactory.createEmptyBorder (0, 5, 0, 0));
            add ("West", aComp);
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
    }
}
