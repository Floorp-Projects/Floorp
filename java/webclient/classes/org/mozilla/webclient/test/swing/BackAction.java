
package org.mozilla.webclient.test.swing;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.webclient.*;

class BackAction extends AbstractAction {
    private SwingEmbeddedMozilla em;

        public BackAction(SwingEmbeddedMozilla em) {
            super ("Back");
	    this.em = em;
        }

        public void actionPerformed(ActionEvent evt) {
           System.out.println("Back was pressed!!");
	   em.back();

        }
    }
