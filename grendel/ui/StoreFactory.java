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
 * Created: Will Scullin <scullin@netscape.com>, 19 Nov 1997.
 */

package grendel.ui;

import java.util.ResourceBundle;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.mail.AuthenticationFailedException;
import javax.mail.MessagingException;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.URLName;

import javax.swing.JOptionPane;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.event.EventListenerList;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import grendel.view.ViewedStore;
import grendel.view.ViewedStoreBase;

import java.lang.ClassNotFoundException;
import java.lang.NoSuchMethodException;
import java.lang.IllegalAccessException;
import java.lang.IllegalArgumentException;
import java.lang.InstantiationException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;


public synchronized class StoreFactory {
  static StoreFactory fInstance = null;

  Session fSession = null;
  ViewedStore[] fStores = null;
  DialogAuthenticator fAuthenticator = null;
  EventListenerList fListeners = new EventListenerList();

  private StoreFactory() {
    Preferences prefs = PreferencesFactory.Get();
    fAuthenticator = new DialogAuthenticator();
    // ### Remember to put this back when authentication is set up
    // ### otherwise we have a security problem  (talisman)
    fSession =
      //      Session.getDefaultInstance(prefs.getAsProperties(),
      //                           fAuthenticator);
      Session.getDefaultInstance(prefs.getAsProperties(), null);
    fSession.setDebug(true);
  }

  public static StoreFactory Instance() {
    if (fInstance == null) {
      fInstance = new StoreFactory();
    }
    return fInstance;
  }

  public Session getSession() {
    return fSession;
  }

  public ViewedStore[] getStores() {
    if (fStores == null) {
      updateStores();
    }

    return fStores;
  }

  ViewedStore createStore(String storename) {
    ResourceBundle labels = ResourceBundle.getBundle("grendel.ui.Labels");
    URLName urlName = null;

    String proto;

    if (storename.indexOf(":") != -1) {
      urlName = new URLName(storename);
    } else {
      urlName = new URLName(storename, null, -1, null, null, null);
    }

    proto = urlName.getProtocol();

    // ### Very wrong temporary hack -- special case "berkeley" and "pop3",
    // since they doesn't play with the proper registration mechanism right
    // now.
    Store store = null;
    ViewedStore viewedStore = null;

    try {
      Class c = null;
      if (proto.equalsIgnoreCase("berkeley")) {
        c = Class.forName("grendel.storage.BerkeleyStore");
      } else if (proto.equalsIgnoreCase("pop3")) {
        c = Class.forName("grendel.storage.PopStore");
      } else if (proto.equalsIgnoreCase("news")) {
        c = Class.forName("grendel.storage.NewsStore");
      }

      if (c != null) {
        Object args[] = { fSession, urlName };
        Class types[] = { args[0].getClass(), args[1].getClass() };
        Constructor cc = c.getConstructor(types);
        store = (Store) cc.newInstance(args);
      }
    } catch (ClassNotFoundException c) {        // Class.forName
    } catch (NoSuchMethodException c) {         // Constructor.getConstructor
    } catch (IllegalAccessException c) {        // Constructor.newInstance
    } catch (IllegalArgumentException c) {      // Constructor.newInstance
    } catch (InstantiationException c) {        // Constructor.newInstance
    } catch (InvocationTargetException c) {     // Constructor.newInstance
    }

    if (store == null) {
      try {
        store = fSession.getStore(proto);
      } catch (MessagingException e) {
        System.out.println("Got exception " + e +
                           " while creating store of type" + proto);
        e.printStackTrace();
      }
    }

    viewedStore = new ViewedStoreBase(store,
                                      urlName.getProtocol(),
                                      urlName.getHost(),
                                      urlName.getPort(),
                                      urlName.getUsername());
    return viewedStore;
  }

  public ViewedStore getViewedStore(Store aStore) {
    if (fStores == null) {
      return null;
    }

    for (int i = 0; i < fStores.length; i++) {
      if (fStores[i].getStore().equals(aStore)) {
        return fStores[i];
      }
    }
    return null;
  }

  boolean SafeEquals(Object a, Object b) {
    if (a == b) {
      return true;
    } else if (a != null && b != null) {
      return a.equals(b);
    } else {
      return false;
    }
  }

  public ViewedStore findStore(String storename) {
    URLName urlName = null;

    if (storename.indexOf(":") != -1) {
      urlName = new URLName(storename);
    } else {
      urlName = new URLName(storename, null, -1, null, null, null);
    }
    for (int i = 0; fStores != null && i < fStores.length; i++) {
      if (urlName.getProtocol().equals(fStores[i].getProtocol())) {
        if (SafeEquals(urlName.getHost(), fStores[i].getHost()) &&
            SafeEquals(urlName.getUsername(), fStores[i].getUsername()) &&
            //SafeEquals(urlName.getFile(), fStores[i].getFile()) &&
            urlName.getPort() == fStores[i].getPort()) {
          return fStores[i];
        }
      }
    }
    return null;
  }

  void updateStores() {
    if (fSession == null) {
      getSession();
    }

    Preferences prefs = PreferencesFactory.Get();
    Vector resVector = new Vector();

    String defStore = "";
    if (!prefs.getString("mail.directory","").equals("")) {
      defStore = "berkeley";
    }

    String storelist = prefs.getString("mail.storelist", defStore);
    StringTokenizer st = new StringTokenizer(storelist, " ,;");

    while (st.hasMoreTokens()) {
      String storename = st.nextToken().trim();

      ViewedStore viewedStore = null;

      viewedStore = findStore(storename);
      if (viewedStore == null) {
        viewedStore = createStore(storename);
        System.out.println("created " + viewedStore);
      } else {
        System.out.println("recycled " + viewedStore);
      }

      if (viewedStore != null) {
        resVector.addElement(viewedStore);
      }
    }
    fStores = new ViewedStore[resVector.size()];
    resVector.copyInto(fStores);
  }

  public void refreshStores() {
    updateStores();

    Object[] listeners = fListeners.getListenerList();
    ChangeEvent event = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      // Lazily create the event:
      if (event == null)
        event = new ChangeEvent(this);
      ((ChangeListener)listeners[i+1]).stateChanged(event);
    }
  }

  public void addChangeListener(ChangeListener l) {
    fListeners.add(ChangeListener.class, l);
  }

  public void removeChangeListener(ChangeListener l) {
    fListeners.remove(ChangeListener.class, l);
  }
}
