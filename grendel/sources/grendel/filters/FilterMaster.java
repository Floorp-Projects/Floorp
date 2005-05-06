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
 * Contributor(s):
 *
 * Class FilterMaster.
 *
 * Created: David Williams <djw@netscape.com>,  1 Oct 1997.
 */
package grendel.filters;

import java.io.Reader;
import java.io.FileReader;
import java.io.File;
import java.io.EOFException;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.util.Hashtable;
import java.util.Enumeration;

import javax.mail.Flags;
import javax.mail.Folder;
import javax.mail.Message;
import javax.mail.Store;
import javax.mail.Session;
import javax.mail.MessagingException;
import javax.mail.search.SearchTerm;
import javax.mail.search.SubjectTerm;
import javax.mail.search.FromTerm;
import javax.mail.event.MessageCountEvent;

import grendel.storage.BerkeleyStore;

import grendel.ui.StoreFactory;

import grendel.filters.FilterSyntaxException;
import grendel.filters.MoveFilterActionFactory;
import grendel.filters.DeleteFilterActionFactory;
import grendel.filters.SubjectTermFactory;

public class FilterMaster extends Object {
  // class slots
  static private FilterMaster fTheMaster = null;

  // class methods
  public static FilterMaster Get() {
        if (fTheMaster == null)
          fTheMaster = new FilterMaster();
        return fTheMaster;
  }

  // instance slots
  private Hashtable fFilters; // to hold named filters
  private Hashtable fFilterActionFactories;
  private Hashtable fFilterTermFactories;

  // constructor API
  private FilterMaster() {
        fFilters = new Hashtable();
        fFilterTermFactories = new Hashtable();
        fFilterActionFactories = new Hashtable();

        // Register built-in terms.
        registerFilterTermFactory(new SubjectTermFactory());

        // Register built-in filter actions.
        registerFilterActionFactory(new MoveFilterActionFactory());
        registerFilterActionFactory(new DeleteFilterActionFactory());

        // Locate and read the filters description file.
    String home_dir = System.getProperties().getProperty("user.home");
        File   home_dir_file = new File(home_dir);
        File   mail_filters_file = new File(home_dir_file, "mail.filters");
        FileReader fr = null;
        try {
          fr = new FileReader(mail_filters_file);
        } catch (FileNotFoundException e) {
          System.err.println("mail.filters not found, skipping loadFilters()");
          fr = null;
        }

        // We must do this as loadFilters calls Get(), and we don't want to
        // go recursive. This is a reasonable thing to do, because at this
        // point, the constructor has succeeded, so we know we will be the
        // FilterMaster.
        fTheMaster = this;

        if (fr != null)
          loadFilters(fr);
  }

  // instance methods
  private void loadFilters(Reader reader) {

        IFilter filter;

        FilterRulesParser parser = new FilterRulesParser(reader);

        try {
          while ((filter = parser.getNext()) != null) {
                String name = filter.toString();
                // System.err.println("got filter: " + name);
                fFilters.put(name, filter);
          }
        } catch (FilterSyntaxException e) {
          System.err.println("Error in filter rules: " + e);
        } catch (IOException e) {
          System.err.println("IO error reading filter rules: " + e);
        }

        parser = null;
  }
  /*package*/
  void registerFilterActionFactory(IFilterActionFactory factory) {
        String key = factory.getName();
        if (fFilterActionFactories.get(key) != null) {
          // FIXME
          System.err.println("FilterMaster.registerFilterAction(): duplicate");
          return;
        }
        fFilterActionFactories.put(key, factory);
  }
  /*package*/
  IFilterActionFactory getFilterActionFactory(String name) {
        return (IFilterActionFactory)fFilterActionFactories.get(name);
  }
  /*package*/
  void registerFilterTermFactory(IFilterTermFactory factory) {
        String key = factory.getName();
        if (fFilterTermFactories.get(key) != null) {
          // FIXME
          System.err.println("FilterMaster.registerFilterTerm(): duplicate");
          return;
        }
        fFilterTermFactories.put(key, factory);
  }
  /*package*/
  IFilterTermFactory getFilterTermFactory(String name) {
        return (IFilterTermFactory)fFilterTermFactories.get(name);
  }

  // tell me what you can do. Something like this will need to be created
  // so that you can build a UI. Caller uses object.toString() on the
  // elements to create user visible labels, etc..
  public Enumeration getFilterActionFactories() {
        return fFilterActionFactories.elements();
  }
  public Enumeration getFilterTermFactories() {
        return fFilterTermFactories.elements();
  }
  public Enumeration getFilters() {
        return fFilters.elements();
  }

  // The verb that this whole sub-system is about. Apply all the defined
  // filters to the specified message.
  public void applyFilters(Message message) {
        boolean deleted = false;
        try {
          deleted = message.isSet(Flags.Flag.DELETED);
        } catch (MessagingException e) {
          deleted = false;
        }
        if (deleted)
          return; // don't need to be here

        Enumeration  filters = getFilters();
        while (filters.hasMoreElements()) {
          IFilter filter = (IFilter)filters.nextElement();
          if (filter.match(message)) {
                filter.exorcise(message);
                break; // only do first match
          }
        }
  }
  // Utilities:
  public Folder getFolder(String name) {
        Session session = StoreFactory.Instance().getSession();
        // ### Definitely wrong:
    //XXXrlk: WARNING, DANGEROUS, BAD, GUARANTEED CRASH HERE
    //   temporary change to make Grendel compile
    Store store = null;
        Folder folder;

        try {
          folder = store.getFolder(name);
          if (!folder.exists())
                folder = null;
        } catch (MessagingException e) {
          folder = null;
        }
        return folder;
  }
  // For Testing.
  public void applyFiltersToTestInbox() {

        Folder inbox = getFolder("TestInbox");

        if (inbox != null) {
          synchronized (inbox) {
                Message[] messages;
                try {
                  messages = inbox.getMessages();
                } catch (MessagingException e) {
                  messages = null;
                }
                if (messages != null) {
                  for (int i = 0; i < messages.length; i++) {
                        applyFilters(messages[i]);
                  }
                }
          }
        }
  }
}
