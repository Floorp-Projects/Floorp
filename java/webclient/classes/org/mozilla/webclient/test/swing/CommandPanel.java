package org.mozilla.webclient.test.swing;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import org.mozilla.webclient.*;

public class CommandPanel extends JPanel implements ActionListener {
    private JToolBar urltool;
    private JToolBar navtool;
    private JTextField urlfield;
    private JComboBox cb;
    private SwingEmbeddedMozilla em;

    public CommandPanel(SwingEmbeddedMozilla em) {
        super();
	GridBagLayout gbl = new GridBagLayout();
	GridBagConstraints gbc = new GridBagConstraints();
	setLayout(gbl);
	this.em = em;

	navtool = createNavToolBar();
	urltool = createURLToolBar();

	// Layout of the toolbars.
	gbc.anchor = GridBagConstraints.NORTHWEST;
	add(navtool, gbc);

	gbc.gridwidth = GridBagConstraints.REMAINDER;
	gbc.weightx = 1.0;
	add(Box.createHorizontalGlue(), gbc);
	
	gbc.gridwidth = 1;
	add(urltool, gbc);
    }
 
    private JButton makeButton(String arg) {
        JButton button = new JButton(arg);
        return button;
    }

    private JToolBar createURLToolBar() {
       JToolBar tb = new JToolBar();
       // Ann - commented out the combobox for demo purposes. 9/20/99
       // String temp[] = {"Fee", "Fi", "Fo", "Fum"};
       //cb = new JComboBox(temp);
       //cb.setEditable(true);
       //cb.setLightWeightPopupEnabled(false);
       urlfield = new JTextField(25);
       
       tb.add(urlfield);
       //tb.add(cb);
       //cb.addActionListener(this);
       urlfield.addActionListener(this);
       return tb;
    }

   


    private JToolBar createNavToolBar() {
        JToolBar tb = new JToolBar();

	tb.add(new BackAction(em));
	tb.add(new ForwardAction(em));
	tb.add(new StopAction(em));
	tb.add(new RefreshAction(em));
        tb.putClientProperty("JToolBar.isRollover", Boolean.TRUE);
        return tb;
    }


    public JTextField getURLField() {
	return urlfield;
    }

    public JToolBar getNavToolBar() {
	return navtool;
    }
    
    public void actionPerformed (ActionEvent evt) {
	String command = evt.getActionCommand();
	String url = null;
        System.out.println( "ActionComand is  "+ command);
        if ("comboBoxChanged".equals(command)) {
            url = (String) cb.getSelectedItem();
        } else {
            url = urlfield.getText();
        }
	em.loadURL(url);
    }
    
}
