
package org.mozilla.webclient.test.swing;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.webclient.*;

class RefreshAction extends AbstractAction {
    private SwingEmbeddedMozilla em;

        public RefreshAction(SwingEmbeddedMozilla em) {
            super ("Refresh");
	    this.em = em;
        }

        public void actionPerformed(ActionEvent evt) {
           System.out.println("Refresh was pressed!!");
	   em.refresh();

        }
    }
