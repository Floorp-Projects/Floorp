/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL
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
 * Created: Will Scullin <scullin@netscape.com>, 19 Nov 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Edwin Woudt <edwin@woudt.nl>
 */
package grendel.ui;

import grendel.prefs.base.ServerArray;
import grendel.prefs.base.ServerStructure;

import grendel.view.ViewedStore;
import grendel.view.ViewedStoreBase;

import java.util.Properties;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.URLName;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.EventListenerList;


public class StoreFactory
{
  static StoreFactory fInstance=null;
  DialogAuthenticator fAuthenticator=null;
  EventListenerList fListeners=new EventListenerList();
  Session fSession=null;
  ViewedStore[] fStores=null;

  private StoreFactory()
  {
    //Preferences prefs = PreferencesFactory.Get();
    //fAuthenticator = new DialogAuthenticator();
    //fSession = Session.getDefaultInstance(prefs.getAsProperties(),
    //                             fAuthenticator);
    fSession=Session.getDefaultInstance(new Properties(), null);
    fSession.setDebug(true);
  }

  public synchronized static StoreFactory Instance()
  {
    if (fInstance==null) {
      fInstance=new StoreFactory();
    }

    return fInstance;
  }

  public Session getSession()
  {
    return fSession;
  }

  public synchronized ViewedStore[] getStores()
  {
    if (fStores==null) {
      updateStores();
    }

    return fStores;
  }

  public void addChangeListener(ChangeListener l)
  {
    fListeners.add(ChangeListener.class, l);
  }

  public synchronized void refreshStores()
  {
    updateStores();

    Object[] listeners=fListeners.getListenerList();
    ChangeEvent event=null;

    for (int i=0; i<(listeners.length-1); i+=2) {
      // Lazily create the event:
      if (event==null) {
        event=new ChangeEvent(this);
      }

      ((ChangeListener) listeners[i+1]).stateChanged(event);
    }
  }

  public void removeChangeListener(ChangeListener l)
  {
    fListeners.remove(ChangeListener.class, l);
  }

  private synchronized void closeStores()
  {
    for (int i=0; i<fStores.length; i++) {
      try {
        fStores[i].getStore().close();
      } catch (MessagingException mex) {
        mex.printStackTrace();
      }
    }
  }

  private synchronized ViewedStore createStore(int ID)
  {
    ServerStructure prefs=ServerArray.GetMaster().get(ID);

    Store store=null;
    ViewedStore viewedStore=null;

    try {
      store=grendel.javamail.JMProviders.getJMProviders().getStore(prefs.getAccount());
    } catch (MessagingException e) {
      System.out.println(
        "Got exception "+e+" while creating store for account "+prefs.getAccount());
      e.printStackTrace();
    }

    viewedStore=new ViewedStoreBase(store, ID);

    return viewedStore;
  }

  private synchronized void updateStores()
  {
    if (fSession==null) {
      getSession();
    }

    if (fStores!=null) {
      closeStores();
    }

    ServerArray prefs=ServerArray.GetMaster();

    fStores=new ViewedStore[prefs.size()];

    for (int i=0; i<prefs.size(); i++) {
      fStores[i]=createStore(i);
    }
  }
}
