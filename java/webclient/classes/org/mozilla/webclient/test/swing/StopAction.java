
package org.mozilla.webclient.test.swing;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.webclient.*;

class StopAction extends AbstractAction {
    private SwingEmbeddedMozilla em;

        public StopAction(SwingEmbeddedMozilla em) {
            super ("Stop");
	    this.em = em;
        }

        public void actionPerformed(ActionEvent evt) {
           System.out.println("Stop was pressed!!");
	   em.stop();

        }
    }
