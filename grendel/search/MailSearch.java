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
 * Created: Will Scullin <scullin@netscape.com>,  9 Oct 1997.
 */

package grendel.search;

import java.awt.Component;
import java.text.DateFormat;
import java.text.ParseException;
import java.util.Date;

import javax.mail.Folder;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.AddressException;
import javax.mail.search.DateTerm;
import javax.mail.search.FromTerm;
import javax.mail.search.SentDateTerm;
import javax.mail.search.SubjectTerm;
import javax.mail.search.SearchTerm;

import javax.swing.JTextField;

import grendel.ui.FolderCombo;
import grendel.ui.FolderFrame;
import grendel.ui.FolderPanel;
import grendel.ui.MessageDisplayManager;

import grendel.storage.SearchResultsFolderFactory;
import grendel.view.ViewedFolder;

public class MailSearch implements ISearchable {
  static ISearchAttribute fAttributes[] = {
    new SubjectAttribute(),
    new AuthorAttribute(),
    new SentDateAttribute()};

  FolderCombo fTarget = null;

  public int getSearchAttributeCount() {
    return fAttributes.length;
  }

  public ISearchAttribute getSearchAttribute(int idx) {
    return fAttributes[idx];
  }

  public void search(SearchTerm aTerm) {
    ViewedFolder folder = fTarget.getSelectedFolder();

    // Set target folder focus and search term in SearchResults singleton.
    SearchResultsFolderFactory.SetTarget(folder.getFolder(), aTerm);

    // Now need to force re-opening of the SearchResults folder in
    // Master Panel.... Will?
    Folder results = SearchResultsFolderFactory.Get();
    //    MessageDisplayManager.GetDefaultManager().displayFolder(results);
    FolderFrame frame = FolderFrame.FindFolderFrame(results);
    if (frame == null) {
      frame = new FolderFrame(results);
      frame.setVisible(true);
    }
    frame.toFront();
    frame.requestFocus();
  }

  public ISearchResults getSearchResults() {
    return null;
  }

  public Component getTargetComponent() {
    if (fTarget == null) {
      fTarget = new FolderCombo();
    }

    fTarget.populate(Folder.HOLDS_MESSAGES, 0);

    return fTarget;
  }

  public Component getResultComponent(ISearchResults aResults) {
    Folder results = SearchResultsFolderFactory.Get();
    //FolderPanel resultPanel = new FolderPanel();
    //resultPanel.setFolder(results);
    FolderPanel resultPanel = null;
    return resultPanel;
  }
}

class SubjectAttribute implements ISearchAttribute {

  static final String kID = "Subject";
  static final String kContains = "Contains";

  public String getName() {
    return "Subject";
  }

  public Object getID() {
    return kID;
  }

  public int getOperatorCount() {
    return 1;
  }

  public Object getOperator(int aIndex) {
    return kContains;
  }

  public Component getValueComponent() {
    return new JTextField(30);
  }

  public Object getValue(Component aComponent) {
    return ((JTextField) aComponent).getText();
  }

  public SearchTerm getAttributeTerm(Object aOperatorID, Object aValue) {
    return new SubjectTerm((String) aValue);
  }

  public String toString() {
    return getName();
  }
}

class AuthorAttribute implements ISearchAttribute {

  static final String kID = "Author";
  static final String kIs = "Is";

  public String getName() {
    return "Author";
  }

  public Object getID() {
    return kID;
  }

  public int getOperatorCount() {
    return 1;
  }

  public Object getOperator(int aIndex) {
    return kIs;
  }

  public Component getValueComponent() {
    return new JTextField(30);
  }

  public Object getValue(Component aComponent) {
    return ((JTextField) aComponent).getText();
  }

  public SearchTerm getAttributeTerm(Object aOperatorID, Object aValue) {
    try {
      return new FromTerm(new InternetAddress((String) aValue));
    } catch (AddressException e) {
      return null;
    }
  }

  public String toString() {
    return getName();
  }
}

class SentDateAttribute implements ISearchAttribute {

  static final String kID = "SendDate";
  static final String kLessThan = "is before";
  static final String kIs = "is";
  static final String kGreaterThan = "is after";

  static String sOperators[] = {kIs, kLessThan, kGreaterThan};

  public String getName() {
    return "Sent Date";
  }

  public Object getID() {
    return kID;
  }

  public int getOperatorCount() {
    return sOperators.length;
  }

  public Object getOperator(int aIndex) {
    return sOperators[aIndex];
  }

  public Component getValueComponent() {
    return new JTextField(15);
    //    return new DateChooser(DateChooser.DATE_MODE, DateChooser.SMALL);
  }

  public Object getValue(Component aComponent) {
    Date res = null;
    try {
      res =
        DateFormat.getDateInstance().parse(((JTextField)aComponent).getText());
    } catch (ParseException e) {
    }
    return res;
  }

  public SearchTerm getAttributeTerm(Object aOperatorID, Object aValue) {
    int op = aOperatorID.equals(kLessThan) ? DateTerm.LT :
      (aOperatorID.equals(kGreaterThan) ? DateTerm.GT : DateTerm.EQ);
    return new SentDateTerm(op, (Date) aValue);
  }

  public String toString() {
    return getName();
  }
}

