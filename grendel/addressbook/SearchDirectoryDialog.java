

package grendel.addressbook;

import java.awt.*;
import java.awt.event.*;
import java.util.*;

import javax.swing.*;

public class SearchDirectoryDialog extends JDialog {

  JTextField eDescription;
  JTextField eLDAPServer;
  JTextField eSearchRoot;
  JTextField ePortNumber;
  JTextField eResultsToShow;
  JCheckBox cSecure;
  JCheckBox cLoginPassword;
  JCheckBox cSavePassword;

  public SearchDirectoryDialog(Frame aParent) {
    //FIX: Resource
    super(aParent, "Directory Server Property", true);

    setResizable(true);
    setSize (300, 515);

    addWindowListener(new AppCloser());

    JTabbedPane tabbedPane = new JTabbedPane();

    getContentPane().add(tabbedPane, BorderLayout.CENTER);

    JComponent generalPanel = createGeneralPanel ();
    tabbedPane.addTab("General",null,generalPanel,"General Properties");
/*
    JComponent offLinePanel = createOfflinePanel ();
    tabbedPane.addTab("Offline Settings",null,offLinePanel,"Offline Settings");
*/

    JButton bOK = new JButton("OK");
    bOK.addActionListener(new OKEvents());

    JButton bCancel = new JButton("Cancel");
    bCancel.addActionListener(new CancelEvents());

    JPanel pOKCancel = new JPanel();
    pOKCancel.add(bOK);
    pOKCancel.add(bCancel);

    getContentPane().add(pOKCancel, BorderLayout.SOUTH);

    getRootPane().setDefaultButton(bOK);
  }

  protected final class AppCloser extends WindowAdapter {
    public void windowClosing(WindowEvent e) {
        dispose();
    }
  }

  protected final class OKEvents implements ActionListener{
      public void actionPerformed(ActionEvent e) {
         dispose();
      }
  }

  protected final class CancelEvents implements ActionListener{
      public void actionPerformed(ActionEvent e) {
         dispose();
      }
  }

  private JPanel createGeneralPanel () {
      //the outer most panel has groove etched into it.
      JPanel pane = new JPanel(false);
      pane.setLayout (new FlowLayout(FlowLayout.LEFT));

      JPanel generalPane = new JPanel(false);
      generalPane.setLayout (new GridLayout (3,2));

      eDescription        = makeField ("Description:", 20, generalPane);
      eLDAPServer = makeField ("LDAP Server:", 20, generalPane);
      eSearchRoot   = makeField ("Search Root:", 20, generalPane);

      pane.add (generalPane);

      JPanel configurationPane = new JPanel(false);
      configurationPane.setLayout (new GridLayout(2,1));

      JPanel portNumberPane = new JPanel(false);
      portNumberPane.setLayout (new GridLayout(1,2));
      ePortNumber      = makeField ("Port Number:", 20, portNumberPane);
      configurationPane.add(portNumberPane);

      JPanel resultsToShowPane = new JPanel(false);
      resultsToShowPane.setLayout (new FlowLayout(FlowLayout.LEFT));
      eResultsToShow   = makeField ("Don't show more than", 3, resultsToShowPane);
      JLabel aLabel = new JLabel ("results");
      resultsToShowPane.add (aLabel);

      configurationPane.add(resultsToShowPane);

      pane.add (configurationPane);

      return pane;
  }

  private JTextField makeField (String aTitle, int aCol, JPanel aPanel) {
      JLabel title = new JLabel (aTitle);
      aPanel.add (title);

      JTextField textField = new JTextField (aCol);
      aPanel.add (textField);

      return textField;
  }
}

