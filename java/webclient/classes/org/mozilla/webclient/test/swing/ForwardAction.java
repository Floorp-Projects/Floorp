
package org.mozilla.webclient.test.swing;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.webclient.*;

class ForwardAction extends AbstractAction {
    private SwingEmbeddedMozilla em;

        public ForwardAction(SwingEmbeddedMozilla em) {
            super ("Forward");
	    this.em = em;
        }

        public void actionPerformed(ActionEvent evt) {
           System.out.println("Forwardwas pressed!!");
	   em.forward();

        }
    }
