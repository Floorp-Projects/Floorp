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
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.composition;

import java.awt.*;
import javax.swing.*;

public class AddressBar extends NSTabbedPane {
    AddressList mAddressList;
    AttachmentsList mAttachmentsList;
    OptionsPanel mOptionsPanel;

    public AddressBar(CompositionPanel cp) {
      // address panel
        ImageIcon addressIcon = new ImageIcon(getClass().getResource("images/small_address.gif"));
        mAddressList = new AddressList ();

          // attachments panel
        ImageIcon attachmentsIcon = new ImageIcon(getClass().getResource("images/small_attachments.gif"));
        mAttachmentsList = new AttachmentsList();

          // options panel
        ImageIcon optionsIcon = new ImageIcon(getClass().getResource("images/small_otpions.gif"));
        mOptionsPanel = new OptionsPanel(cp);

        // tabbed panel holds address, attachments and otpions.
        addTab("", addressIcon,        mAddressList);
        addTab("", attachmentsIcon,    mAttachmentsList);
        addTab("", optionsIcon,        mOptionsPanel);
        setSelectedIndex(0);

        setBackgroundAt(0, Color.lightGray);
        setBackgroundAt(1, Color.lightGray);
        setBackgroundAt(2, Color.lightGray);

        setTabPlacement(LEFT);
       
    }

    public AddressList getAddressList() {
        return mAddressList;
    }

    public AttachmentsList getAttachmentsList() {
        return mAttachmentsList;
    }

    public OptionsPanel getOptionsPanel() {
        return mOptionsPanel;
    }
}
