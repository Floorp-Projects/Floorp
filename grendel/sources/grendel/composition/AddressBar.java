/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Jeff Galyan <talisman@anamorphic.com>
 *                 Edwin Woudt <edwin@woudt.nl>
 *                 Brian Duff <Brian.Duff@oracle.com>
 */

package grendel.composition;

import java.awt.*;
import javax.swing.*;

/**
 * The address bar is displayed at the top of the message composition panel.
 * It consists of a split pane, on the left of which is the From: field 
 * (identity), address list and subject field, and on the right of which
 * is the attachements list.
 */
public class AddressBar extends JPanel {
    AddressList mAddressList;
    AttachmentsList mAttachmentsList;
    OptionsPanel mOptionsPanel;
    

    public AddressBar(CompositionPanel cp) {
        super();
        
        setLayout(new BorderLayout());
    
        JSplitPane splitPane = new JSplitPane();
    
        // options panel
        mOptionsPanel = new OptionsPanel(cp);
            
        // address panel
        mAddressList = new AddressList ();
        
        JPanel leftPanel = new JPanel();
        leftPanel.setLayout(new BorderLayout(3, 3));
        leftPanel.add(mOptionsPanel, BorderLayout.NORTH);
        leftPanel.add(mAddressList, BorderLayout.CENTER);
        
        splitPane.setLeftComponent(leftPanel);

        // attachments panel
        mAttachmentsList = new AttachmentsList();
        
        JPanel rightPanel = new JPanel();
        JLabel attachLabel = new JLabel("Attachments:");
        rightPanel.setLayout(new BorderLayout(3,3));
        rightPanel.add(attachLabel, BorderLayout.NORTH);
        rightPanel.add(mAttachmentsList, BorderLayout.CENTER);
        
        splitPane.setRightComponent(rightPanel);
        
        add(splitPane, BorderLayout.CENTER);
       
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
