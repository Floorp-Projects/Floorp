/*
 * Copyright (C) 2004 Sun Microsystems, Inc. All rights reserved. Use is
 * subject to license terms.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */ 

package org.mozilla.webclient.test;

import javax.swing.*;
import java.awt.*;


/**
 * JDIC API demo class.
 * <p>
 * The class represents a status bar.
 */

class MyStatusBar extends Box {
    public JLabel lblStatus, lblDesc;

    public MyStatusBar() {
        super(BoxLayout.X_AXIS);

        Toolkit kit = Toolkit.getDefaultToolkit();
        Dimension screenSize = kit.getScreenSize();

        // Add the JLabel displaying the selected object numbers.
        lblStatus = new JLabel("Status:", SwingConstants.LEADING);
        lblStatus.setPreferredSize(new Dimension((int) (0.7 * screenSize.width),
                22));
        lblStatus.setBorder(BorderFactory.createLoweredBevelBorder());
        this.add(lblStatus, null);

        // Add the JLabel displaying the selected object size.
        // lblSize = new JLabel("Size:", SwingConstants.LEADING);
        // lblSize.setPreferredSize(new Dimension((int)(0.2*screenSize.width), 22));
        // lblSize.setBorder(BorderFactory.createLoweredBevelBorder());
        // this.add(lblSize, null);

        // Add the JLabel displaying the description.
        lblDesc = new JLabel("Description:", SwingConstants.LEADING);
        lblDesc.setPreferredSize(new Dimension((int) (0.3 * screenSize.width),
                22));
        lblDesc.setBorder(BorderFactory.createLoweredBevelBorder());
        this.add(lblDesc, null);
    }
}
