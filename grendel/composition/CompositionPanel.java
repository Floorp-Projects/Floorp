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
 * Created: Will Scullin <scullin@netscape.com>, 20 Oct 1997.
 *
 * Contributors: Jeff Galyan <jeffrey.galyan@sun.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 *               Thomas Down <thomas.down@tri.ox.ac.uk>
 */

package grendel.composition;

import calypso.util.ByteBuf;
import calypso.util.ByteLineBuffer;
import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import java.awt.BorderLayout;
import java.awt.Container;
import java.awt.FileDialog;
import java.awt.Font;
import java.awt.Frame;
import java.awt.Insets;
import java.awt.Toolkit;
import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.awt.event.ActionEvent;
import java.io.*;
import java.net.URL;
import java.util.Hashtable;
import java.util.StringTokenizer;
import java.util.Properties;

import javax.swing.AbstractAction;
import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextField;
import javax.swing.JTextArea;
import javax.swing.border.BevelBorder;
import javax.swing.event.ChangeEvent;
import javax.swing.event.EventListenerList;
import javax.swing.text.BadLocationException;
import javax.swing.text.Document;
import javax.swing.text.JTextComponent;
import javax.swing.text.TextAction;

import javax.activation.*;

import javax.mail.Address;
import javax.mail.Session;
import javax.mail.Message;
import javax.mail.Transport;
import javax.mail.MessagingException;
import javax.mail.internet.MimeMessage;
import javax.mail.internet.MimeMultipart;
import javax.mail.internet.MimeBodyPart;
import javax.mail.internet.MimeUtility;
import javax.mail.internet.InternetAddress;

import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;
import grendel.ui.ActionFactory;
import grendel.ui.GeneralPanel;
import grendel.ui.UIAction;
import grendel.widgets.CollapsiblePanel;
import grendel.widgets.GrendelToolBar;

public class CompositionPanel extends GeneralPanel {
  private Hashtable       mCommands;
  private Hashtable       mMenuItems;

  private AddressBar      mAddressBar;
  private AddressList     mAddressList;
  private AttachmentsList mAttachmentsList;
  private JTextComponent  mEditor;
  private JTextField      mSubject;
  private Session         mSession;
  private String          mMailServerHostName;
  private String          mMailUserName;
  private Message referredMsg;

  private EventListenerList mListeners = new EventListenerList();

  /**
   *
   */
  public CompositionPanel(Session aSession) {
    fResourceBase = "grendel.composition";

    mSession = aSession;

    //Create message panel
    //  The MessagePanel is...
    //      1) The subject JLabel
    //      2) The subject JTextField and
    //      3) An editor (JTextArea)
    //  The editor in the message panel must be created first so
    //  the supported JActions can be registered with the menu "mMenubar".
    MessagePanel messagePanel = new MessagePanel ();
    mEditor = messagePanel.getEditor ();
    mSubject = messagePanel.getSubject ();

    //toolbar buttons
    fToolBar = createToolbar();
    mAddressBar = new AddressBar(this);
    mAddressList = mAddressBar.getAddressList();
    mAttachmentsList = mAddressBar.getAttachmentsList();

    //the splitPane holds collapseble panels and message panels.
//         JSplitPane splitPane = new JSplitPane(JSplitPane.VERTICAL_SPLIT, false, collapsePanel, messagePanel);


    add(messagePanel, BorderLayout.CENTER);
  }

  public void dispose() {
  }

  /**
   * Fetch the list of actions supported by this
   * editor.  It is implemented to return the list
   * of actions supported by the embedded JTextComponent
   * augmented with the actions defined locally.
   */
  public UIAction[] getActions() {
    return defaultActions;
    // XXX WHS need to translate Actions to UICmds
    // return TextAction.augmentList(mEditor.getActions(), defaultActions);
  }

  /**
   * Return the addressing bar
   */

  public AddressBar getAddressBar() {
    return mAddressBar;
  }

  void setSubject(String subject) {
    mSubject.setText(subject);
  }

  String getSubject() {
    return mSubject.getText();
  }

  void setReferredMessage(Message m) {
    referredMsg = m;
  }

  Message getReferredMessage() {
    return referredMsg;
  }

  /**
   * Add a CompositionPanelListener
   */

  public void addCompositionPanelListener(CompositionPanelListener l) {
    mListeners.add(CompositionPanelListener.class, l);
  }
  
  /**
   * Remove a CompositionPanelListener
   */

  public void removeCompositionPanelListener(CompositionPanelListener l) {
    mListeners.remove(CompositionPanelListener.class, l);
  }

  /**
   * Quote the orgininal message in a reply
   */
   
  public void QuoteOriginalMessage() {
    QuoteOriginalText qot = new QuoteOriginalText();
    qot.actionPerformed(new ActionEvent(this,0,""));
  }

  /**
   * Add a signature
   */
   
  public void AddSignature() {
    AddSignatureAction asa = new AddSignatureAction();
    asa.actionPerformed(new ActionEvent(this,0,""));
  }

  protected void notifySendingMail() {
    Object[] listeners = mListeners.getListenerList();
    ChangeEvent changeEvent = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      if (listeners[i] == CompositionPanelListener.class) {
        // Lazily create the event:
        if (changeEvent == null)
          changeEvent = new ChangeEvent(this);
        ((CompositionPanelListener)listeners[i+1]).sendingMail(changeEvent);
      }
    }
  }

  protected void notifyMailSent() {
    Object[] listeners = mListeners.getListenerList();
    ChangeEvent changeEvent = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      if (listeners[i] == CompositionPanelListener.class) {
        // Lazily create the event:
        if (changeEvent == null)
          changeEvent = new ChangeEvent(this);
        ((CompositionPanelListener)listeners[i+1]).doneSendingMail(changeEvent);
      }
    }
  }

  protected void notifySendFailed() {
    Object[] listeners = mListeners.getListenerList();
    ChangeEvent changeEvent = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      if (listeners[i] == CompositionPanelListener.class) {
        // Lazily create the event:
        if (changeEvent == null)
          changeEvent = new ChangeEvent(this);
        ((CompositionPanelListener)listeners[i+1]).sendFailed(changeEvent);
      }
    }
  }

  /**
   * Find the hosting frame, for the file-chooser dialog.
   */
  protected Frame getParentFrame() {
    for (Container p = getParent(); p != null; p = p.getParent()) {
      if (p instanceof Frame) {
        return (Frame) p;
      }
    }
    return null;
  }

  //"File" actions
  public static final String saveDraftTag             ="saveDraft";
  public static final String saveAsTag                ="saveAs";
  public static final String sendNowTag               ="sendNow";
  public static final String sendLaterTag             ="sendLater";
  public static final String quoteOriginalTextTag     ="quoteOriginalText";
  public static final String addSignatureTag          ="addSignatureAction";
  public static final String selectAddressesTag       ="selectAddresses";
  public static final String goOfflineTag             ="goOffline";
  public static final String closeWindowTag           ="closeWindow";

  // "file->new" actions
  public static final String navigatorWindowTag       ="navigatorWindow";
  public static final String messageTag               ="message";
  public static final String blankPageTag             ="blankPage";
  public static final String pageFromTemplateTag      ="pageFromTemplate";
  public static final String pageFromWizardTag        ="pageFromWizard";

  // "file->attach" actions
  public static final String fileTag                  ="attachFile";
  public static final String webPageTag               ="webPage";
  public static final String addressBookCardTag       ="addressBookCard";
  public static final String myAddressBookCardTag     ="myAddressBookCard";

  //"Edit" actions
  public static final String undoTag                  ="undo";
  public static final String pasteAsQuotationTag      ="pasteAsQuotation";
  public static final String DeleteTag                ="delete";
  public static final String selectAllTag             ="selectAll";
  public static final String findInMessageTag         ="findInMessage";
  public static final String findAgainTag             ="findAgain";
  public static final String searchDirectoryTag       ="searchDirectory";
  public static final String preferencesTag           ="appPrefs";

  //"View" actions
  public static final String hideMessageToolbarTag    ="hideMessageToolbar";
  public static final String hideAddressingAreaTag    ="hideAddressingArea";
  public static final String viewAddressTag           ="viewAddress";
  public static final String viewAttachmentsTag       ="viewAttachments";
  public static final String viewOptionsTag           ="viewOptions";
  public static final String wrapLongLinesTag         ="wrapLongLines";

  // --- action implementations -----------------------------------
  private UIAction[] defaultActions = {
    //"File" actions
//        new SaveDraft(),
    new SaveAs(),
    new SendNow(),
//        new SendLater(),
    new QuoteOriginalText(),
    new SelectAddresses(),
//        new GoOffline(),
    ActionFactory.GetExitAction(),

    // "file->new" actions
//        new NavigatorWindow(),
//        new Message(),
//        new BlankPage(),
//        new PageFromTemplate(),
//        new PageFromWizard(),

    // "file->attach" actions
    new AttachFile(),
//        new WebPage(),
//        new AddressBookCard(),
//        new MyAddressBookCard(),

    //"Edit" actions
//        new Undo(),
    //new Cut(),    Handled by editor.
    //new Copy(),   Handled by editor.
    //new Paste(),  Handled by editor.
    new PasteAsQuotation(),
//        new Delete(),
//        new FindInMessage(),
//        new FindAgain(),
//        new SelectAll(),
//        new SearchDirectory(),
    ActionFactory.GetPreferencesAction(),

    //"View" actions
//        new HideMessageToolbar(),
//        new HideAddressingArea(),
    new ViewAddress(),
    new ViewAttachments(),
    new ViewOptions(),
//        new WrapLongLines()
  };

  //-----------------------
  //"File" actions
  //-----------------------
  /**
   * Try to save the message to the "draft" mailbox.
   * @see SaveAs
   */
  class SaveDraft extends UIAction {
    SaveDraft() {
      super(saveDraftTag);
      this.setEnabled(true);
    }
    public void actionPerformed(ActionEvent e) {}
  }

  /**
   * Try to save the message to a text file.
   * @see SaveDraft
   */
  class SaveAs extends UIAction {
    SaveAs() {
      super(saveAsTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      FileDialog fileDialog = new FileDialog (getParentFrame(), "Save As", FileDialog.SAVE);
      fileDialog.setFile("untitled.txt");
      fileDialog.show();  //blocks

      String fileName = fileDialog.getFile();
      //check for canel
      if (fileName == null) {
        return;
      }

      //try to retieve all text from message area.
      String messageText = mEditor.getText();

      //try to write text to file.
      try {
        //assemble fill path to file.
        String directory = fileDialog.getDirectory();
        File f = new File(directory, fileName);
        FileOutputStream fstrm = new FileOutputStream(f);

        //write text to the file.
        fstrm.write(messageText.getBytes()) ;
        fstrm.flush();
      } catch (IOException io) {
      }
    }
  }

  /**
   * Send the mail message now.
   */
  class SendNow extends UIAction {
    SendNow() {
      super(sendNowTag);
      this.setEnabled(true);
    }

    /**
     * Check if an array of bytes are in range 1-127 (i.e. clean ASCII)
     */

    public boolean isCleanText(byte [] data) {
      for (int i=0; i < data.length; ++i)
        if (data[i] <= 0)
          return false;

      return true;
    }

    public void actionPerformed(ActionEvent ae) {
      //try to retieve all text from message area.
      notifySendingMail();
      boolean success = false;

      String messageText = mEditor.getText();

      if (messageText == null) {
        System.err.println("****Null messageText");
      }

      //try to send the message
      //get the current list of recipients.
      Addressee[] recipients = mAddressList.getAddresses();

      //Check that is at least one recipient.
      if (0 < recipients.length) {
        //get the sending identity
        Preferences prefs = PreferencesFactory.Get();
        int ident = mAddressBar.getOptionsPanel().getSelectedIdentity();
	String userName = prefs.getString("mail.identity-" + ident + ".username", "Nobody") + " <" 
	                + prefs.getString("mail.identity-" + ident + ".email", "nobody@localhost") + ">";

        if (userName != null) {
          //create a mime message
          MimeMessage msg = new MimeMessage(mSession); // TD

          try {
            //set who's sending this message.
            msg.setFrom (new InternetAddress(userName));

            //add the recipients one at a time.
            for (int i = 0; i < recipients.length; i++) {
              javax.mail.Address[] toAddress = new InternetAddress[1];
              toAddress[0] = new InternetAddress(recipients[i].getText());

              Message.RecipientType deliverMode = Message.RecipientType.TO;

              //map grendel.composition.Addressee delivery modes
              //  into javax.mail.Message delivery modes.
              switch (recipients[i].getDelivery()) {
                case Addressee.TO:
                  deliverMode = Message.RecipientType.TO;
                  break;
                case Addressee.CC:
                  deliverMode = Message.RecipientType.CC;
                  break;
                case Addressee.BCC:
                  deliverMode = Message.RecipientType.BCC;
                  break;
              }
              msg.addRecipients(deliverMode, toAddress);
            }

            msg.setSubject(mSubject.getText());        //set subject from text
                                                       //field.
            msg.setHeader("X-Mailer", "Grendel [development version]");
                                                       //and proud of it! 
            msg.setSentDate(new java.util.Date());     //set date to now.

            String [] attachments = mAttachmentsList.getAttachments();
            if (attachments.length == 0) {
              msg.setContent(messageText, "text/plain"); //contents.
            } else {
              MimeMultipart multi = new MimeMultipart();
              
              MimeBodyPart mainText = new MimeBodyPart();
              mainText.setText(messageText);
              multi.addBodyPart(mainText);

              for (int i = 0; i < attachments.length; ++i) {
                try {
                  File f = new File(attachments[i]);
                  int len = (int) f.length();
                  String mimeString =
                    FileTypeMap.getDefaultFileTypeMap().getContentType(f);
                  MimeType mimeType = new MimeType(mimeString);

                  byte [] bs = new byte[len];
                  FileInputStream fis = new FileInputStream(f);
                  DataInputStream dis = new DataInputStream(fis);
                  dis.readFully(bs);
                  dis.close();
                  fis.close();

                  MimeBodyPart att = new MimeBodyPart();
                  String encName = "7bit";
                  if (mimeType.getPrimaryType().equalsIgnoreCase("text")) {
                    if (!isCleanText(bs)) {
                      encName = "quoted-printable";
                    }
                  } else {
                    encName = "base64";
                  }

                  att.setText(new String(bs)); 
                  att.setHeader("Content-Type", mimeString);
                  att.setHeader("Content-Transfer-Encoding", encName);
                  att.setFileName(new File(attachments[i]).getName());
                  att.setDisposition("Attachment");
                  multi.addBodyPart(att);
                } catch (Exception e) {
                  // Could be IOException or MessagingException.  For now...
                  e.printStackTrace();
                }
              }

              msg.setContent(multi);
            }

            try {
              mSession.getTransport("smtp").send(msg);       // send the message.
            } catch (MessagingException exc) {
              exc.printStackTrace();
            }
                    
            success = true;
          } catch (javax.mail.SendFailedException sex) {
            sex.printStackTrace();
            Address addr[] = sex.getInvalidAddresses();
            if (addr != null) {
              System.err.println("Addresses: ");
              for (int i = 0; i < addr.length; i++) {
                System.err.println("  " + addr[i].toString());
              }
            }
          } catch (MessagingException mex) {
            mex.printStackTrace();
          }
        }  else {
          System.err.println("user.email_address undefined");
        }
      }

      if (success) {
        //hide this frame after sending mail.
        notifyMailSent();
      } else {
        notifySendFailed();
      }
    }
  }

  /**
   * Quote the original text message into the editor.
   * @see PasteAsQuotation
   */
  class QuoteOriginalText extends UIAction {
    QuoteOriginalText() {
      super(quoteOriginalTextTag);
      this.setEnabled(true);
    }
    public void actionPerformed(ActionEvent event) {
      if (referredMsg == null) return; // Or beep or whine??? ###
      // ### Get the message as a stream of text.  This involves
      // complicated things like invoking the MIME parser, and throwing
      // away non-text parts, and translating HTML to text, and so on.
      // Yeah, right.
      InputStream plaintext = null;
      try {
        plaintext = referredMsg.getInputStream();
      } catch (IOException ioe) {
      } catch (MessagingException e) {
      }
      if (plaintext == null) return; // Or beep or whine??? ###


      int position = mEditor.getCaretPosition();
      Document doc = mEditor.getDocument();

      MessageExtra mextra = MessageExtraFactory.Get(referredMsg);
      String author;
      try {
        author = mextra.getAuthor();
      } catch (MessagingException e) {
        author = "???";         // I18N? ###
      }
      String tmp = author + " wrote:\n"; // I18N ###
      try {
        doc.insertString(position, tmp, null);
        position += tmp.length();
      } catch (BadLocationException e) {
      }

      // OK, now insert the data from the plaintext, and precede each
      // line with "> ".

      ByteLineBuffer linebuffer = new ByteLineBuffer();
      ByteBuf empty = new ByteBuf();
      linebuffer.setOutputEOL(empty);

      boolean eof = false;

      ByteBuf buf = new ByteBuf();
      ByteBuf line = new ByteBuf();

      while (!eof) {
        buf.setLength(0);
        try {
          eof = (buf.read(plaintext, 1024) < 0);
        } catch (IOException e) {
          eof = true;
        }
        if (eof) {
          linebuffer.pushEOF();
        } else {
          linebuffer.pushBytes(buf);
        }
        while (linebuffer.pullLine(line)) {
          try {
            doc.insertString(position, "> ", null);
            position += 2;
            doc.insertString(position, line.toString(), null);
            position += line.length();
            doc.insertString(position, "\n", null);
            position += 1;
          } catch (BadLocationException e) {
          }
        }
      }
      repaint();
      try {
        plaintext.close();
      } catch (IOException e) {
      }
    }
  }

  /**
   * Add a signature
   */
  class AddSignatureAction extends UIAction {
    AddSignatureAction() {
      super(addSignatureTag);
      this.setEnabled(true);
    }
    public void actionPerformed(ActionEvent event) {

      int ident = mAddressBar.getOptionsPanel().getSelectedIdentity();
      Preferences prefs = PreferencesFactory.Get();
      String sigFileName = prefs.getString("mail.identity-" + ident + ".signature", "");

      Document doc = mEditor.getDocument();
      int oldPosition = mEditor.getCaretPosition();
      
      for (int i=0; i<doc.getLength()-3;i++) {
        try {
          if (doc.getText(i,1).equals("\n")) {
            if (doc.getText(i+1,3).equals("-- ")) {
              doc.remove(i, doc.getLength()-i);
            }
          }
        } catch (BadLocationException ble) {
          ble.printStackTrace();
        }
      }
      
      int position = doc.getEndPosition().getOffset() - 1;
      
      try {
        BufferedReader sigReader = new BufferedReader(new FileReader(sigFileName));

        String s = "\n-- ";
        while (s != null) {
          s = s + "\n";
          doc.insertString(position, s, null);
          position += s.length();
      	  s = sigReader.readLine();
        }
      } catch (FileNotFoundException fnfe) {
        //this can mean two things: either there's no signature specified
        //or the file is missing. I the last case we should do
        //something sensible.
      } catch (IOException ioe) {
        ioe.printStackTrace();
      } catch (BadLocationException ble) {
        ble.printStackTrace();
      }
      
      mEditor.setCaretPosition(oldPosition);
      
    }
  }

  class SelectAddresses extends UIAction {
    SelectAddresses() {
      super(selectAddressesTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      AddressDialog aDialog = new AddressDialog(getParentFrame());
      Addressee[]     mAddresses;

      //get the current list to hand to the dialog.
      mAddresses = mAddressList.getAddresses ();

      //display the addressee dialog
      aDialog.setAddresses (mAddresses);      //initialize the dialog
      aDialog.show ();                        //blocks

      if (false == aDialog.getCanceled()) {
        //get the addresses from dialog
        mAddresses = aDialog.getAddresses();

        //update addresses panel with data from dialog.
        mAddressList.setAddresses (mAddresses);
      }

      aDialog.dispose();
    }
  }

  //-----------------------
  // "file->new" actions
  //-----------------------

  //-----------------------
  // "file->attach" actions
  //-----------------------
  class AttachFile extends UIAction {
    AttachFile() {
      super(fileTag);
      this.setEnabled(true);
    }

    //display the AttachmentsList and have the file dialog "popup" on them.
    public void actionPerformed(ActionEvent e) {
      mAddressBar.setSelectedIndex(1);
      mAttachmentsList.showDialog();
    }
  }

  //-----------------------
  //"Edit" actions
  //-----------------------

  /**
   * Quote and paste whatever string that's on the clipboard into the editor.
   * @see QuoteOriginalText
   */
  class PasteAsQuotation extends UIAction {
    PasteAsQuotation() {
      super(pasteAsQuotationTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      Clipboard board=Toolkit.getDefaultToolkit().getSystemClipboard();

      /* an instance of the system clipboard */
      Transferable transfer=board.getContents(this);

      // Try to get a string from the clipboard
      try {
        String clipboardData = (String)
          (transfer.getTransferData(DataFlavor.stringFlavor));
        String quotingString = "> ";
        StringBuffer quotedString = new StringBuffer("");
        boolean appendQuotaion = true;

        //Assemble the quoted string.
        StringTokenizer st = new StringTokenizer(clipboardData, "\n", true);
        while (st.hasMoreTokens()) {
          String token = st.nextToken();

          if (appendQuotaion)
            quotedString.append (quotingString);

          appendQuotaion = true;
          quotedString.append (token);

          //if this token is a line then skip inserting a quotation on the line
          if (!token.equals ("\n"))
            appendQuotaion = false;
        }

        //try to insert the quoted string.
        Document doc = mEditor.getDocument();
        try {
          doc.insertString(0 , quotedString.toString(), null);
          repaint();
        } catch (BadLocationException bl) {
        }
      }
      catch (UnsupportedFlavorException ufe){
      }
      catch(IOException ioe){
      }
    }
  }

  //-----------------------
  //"View" actions
  //-----------------------

  class ViewAddress extends UIAction {
    ViewAddress() {
      super(viewAddressTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      mAddressBar.setSelectedIndex(0);
    }
  }

  class ViewAttachments extends UIAction {
    ViewAttachments() {
      super(viewAttachmentsTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      mAddressBar.setSelectedIndex(1);
    }
  }

  class ViewOptions extends UIAction {
    ViewOptions() {
      super(viewOptionsTag);
      this.setEnabled(true);
    }

    public void actionPerformed(ActionEvent e) {
      mAddressBar.setSelectedIndex(2);
    }
  }

  /**
   * Create a Toolbar
   * @see addToolbarButton
   */
  private GrendelToolBar createToolbar() {

    GrendelToolBar toolBar = new GrendelToolBar();

    addToolbarButton(toolBar, new SendNow(),
                     "send",       "Send this message");
    addToolbarButton(toolBar, new QuoteOriginalText(),
                     "quote",      "Quote the previous document");
    addToolbarButton(toolBar, new SelectAddresses(),
                     "address",    "Address this message");
    addToolbarButton(toolBar, new AttachFile(),
                     "attach",     "Include an attachment");
    addToolbarButton(toolBar, null,
                     "spelling",   "Check Spelling");
    addToolbarButton(toolBar, new SaveDraft(),
                     "save",       "Save this message as a draft");
    addToolbarButton(toolBar, null,
                     "security",   "Show security Information");
    addToolbarButton(toolBar, null,
                     "stop",       "Stop the current Transfer (ESC)" );
    //addToolbarButton(toolBar, new AddSignatureAction(),
    //                 "signature",  "Add the signature of the current personality" );

    return toolBar;
  }

  /**
   * create a toolbar button
   * @param aToolBar The parent toolbar to add this button to.
   * @param aActionListener Who you want to be notified when the button is
   * pressed.
   * @param aImageName The image name for the button. like "save.gif"
   * @param aToolTip The buttons tool tip. like "Save the current file".
   * @see createToolbar
   */
  public void addToolbarButton(GrendelToolBar aToolBar,
                               UIAction aActionListener,
                               String aImageName,
                               String aToolTip) {
    JButton b = new JButton();

    b.setHorizontalTextPosition(JButton.CENTER);
    b.setVerticalTextPosition(JButton.BOTTOM);
    b.setRolloverEnabled(true);
    b.setBorder(BorderFactory.createEmptyBorder());
    b.setMargin(new Insets(5,5,5,5));
    b.setToolTipText(aToolTip);

    URL iconUrl = getClass().getResource("images/" + aImageName + ".gif");
    b.setIcon(new ImageIcon(iconUrl));

    iconUrl = getClass().getResource("images/" +
                                     aImageName + "-disabled.gif");
    b.setDisabledIcon(new ImageIcon(iconUrl));

    iconUrl = getClass().getResource("images/" +
                                     aImageName + "-pressed.gif");
    b.setPressedIcon(new ImageIcon(iconUrl));

    iconUrl = getClass().getResource("images/" +
                                     aImageName + "-rollover.gif");
    b.setRolloverIcon(new ImageIcon(iconUrl));


//        JButton b = new JButton(new ImageIcon(aImageName));
//      b.setToolTipText(aToolTip);
//      b.setPad(new Insets(3,3,3,3));

    if (aActionListener != null) {
      b.addActionListener(aActionListener);
    } else {
      b.setEnabled(false);
    }

    aToolBar.add(b);
  }

  /**
   * MessagePanel holds the subject and message body fields.
   * The MessagePanel is...
   *     1) The subject label "Subject:" (JLabel)
   *     2) The subject text field (JTextField)
   *     3) The editor (JTextArea)
   *
   * @see Composition
   */
  class MessagePanel extends JPanel {
    JTextArea   mEditor;
    JTextField  mSubjectText;

    public MessagePanel() {
      super(true);
      setLayout (new BorderLayout());

      //Subject (label and text field)
      JPanel subjectPanel = new JPanel (new BorderLayout(5, 0));
      subjectPanel.setBorder(BorderFactory.createEmptyBorder(5,5,5,5));

      JLabel subjectLabel = new JLabel("Subject:");
      subjectPanel.add (subjectLabel, BorderLayout.WEST);

      mSubjectText = new JTextField("");
      subjectPanel.add (mSubjectText, BorderLayout.CENTER);

      //add the subject panel to this panel.
      add(subjectPanel, BorderLayout.NORTH);

      //message body (text area)
      mEditor = createEditor();
      mEditor.setBorder(new BevelBorder(BevelBorder.LOWERED));
      add(new JScrollPane(mEditor), BorderLayout.CENTER);

      mEditor.setFont(Font.decode("Monospaced-12"));
    }

    /**
     * Creates a text editor for the message panel.
     * @return an editor.
     * @see getEditor
     */
    private JTextArea createEditor () {
      return new JTextArea(0, 70);
    }

    /**
     * return the messsagePanel's subject text field.
     * @return the text field used for the subject.
     */
    public JTextField getSubject () { return mSubjectText; }

    /**
     * return the messsagePanel's editor.
     * @return the editor used for the message panel.
     * @see createEditor
     */
    public JTextArea getEditor () { return mEditor; }
  }

  class PatchedMimeMessage extends MimeMessage {
    PatchedMimeMessage(Session s) {
      super(s);
    }
    public void writeTo(OutputStream o) throws MessagingException
    {
      try {
        if (getContentType().equals("text/plain")) {
          try {
            o.write(new String("Subject: ").getBytes());
            o.write(mSubject.getText().getBytes());
            o.write(new String("\n").getBytes());
            String text = (String) getContent();
            if (text == null) {
              text = mEditor.getText();
            }
            if (text != null) {
              o.write(text.getBytes());
            } else {
              System.err.println("Couldn't writeTo(): null content");
            }
          } catch (IOException e) {
            e.printStackTrace();
            throw new MessagingException("Couldn't writeTo(): " +
                                         e);
          }
        } else {
          super.writeTo(o);
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  }
}
