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

 */


package grendel.integrator;

import java.awt.Image;
import java.awt.Toolkit;
import java.util.Enumeration;
import java.util.Vector;

import javax.activation.DataHandler;

import javax.swing.event.ChangeEvent;
import javax.swing.event.EventListenerList;

//import netscape.shell.IShellIntegrator;
//import netscape.shell.IShellView;
//import netscape.shell.IShellViewCtx;
//import netscape.shell.ShellViewCtxListener;

import javax.mail.Folder;
import javax.mail.MessagingException;

import grendel.view.ViewedFolder;
import grendel.ui.UIFactory;

public class FolderCtx implements IShellViewCtx {
  IShellIntegrator  fIntegrator;
  IShellViewCtx     fParent;
  ViewedFolder      fFolder;
  Vector            fChildren = new Vector();
  DataHandler       fDataHandler;
  EventListenerList fListeners = new EventListenerList();
  boolean           fInited;


  public FolderCtx(ViewedFolder aFolder) {
    fFolder = aFolder;
  }

  /**
   * Initializes this view e.g., the Integrator calls this first so you can
   * identify the view.
   *
   */

  public void initialize( IShellIntegrator shell, IShellViewCtx aParent ) {
    fIntegrator = shell;
    fParent = aParent;
  }

  /**
   * Provides an enumeration for the subviews in this view.
   *
   * @param iFlags Flags determining which items to iclude in the enumeration
   * @see #FOLDERS
   * @see #NONFOLDERS
   * @see #INCLUDEHIDDEN
   *
   * @return An Enumeration for the view context's children
   */
  public Enumeration children( int iFlags ) {
    if (!fInited) {
      initChildren();
    }
    return fChildren.elements();
  }

  /**
   * Provides an array of the view context's children
   *
   * @param iFlags Flags determining which items to iclude in the array
   * @see #FOLDERS
   * @see #NONFOLDERS
   * @see #INCLUDEHIDDEN
   *
   * @return An array of the view context's children
   */
  public IShellViewCtx[] getChildren( int iFlags ) {
    if (!fInited) {
      initChildren();
    }
    IShellViewCtx res[] = new IShellViewCtx[fChildren.size()];
    fChildren.copyInto(res);
    return res;
  }

  /**
   * Returns the number of children for the view context.
   *
   * @param iFlags Flags determining which items to iclude in the array
   * @see #FOLDERS
   * @see #NONFOLDERS
   * @see #INCLUDEHIDDEN
   *
   * @return the number of children for the view context
   */
  public int getChildCount( int iFlags ) {
    if (!fInited) {
      return 1;
    } else {
      return fChildren.size();
    }
  }

  /**
   * Returns the view context's direct parent view context.
   *
   * @return the view context's parent.
   */
  public IShellViewCtx getParent() {
    return fParent;
  }

  /**
   * Sets the view context's parent.
   *
   * @param viewCtx the parent of this view context.
   */
  public void setParent( IShellViewCtx viewCtx ) {
    fParent = viewCtx;
  }

  /**
   * Compares two subviews.
   *
   * @param subview1 identifies the first subview to compare
   * @param subview2 identifies the second subview to compare
   *
   * @return Less than zero     - The first subview should precede the second
   *         Greater than zero  - The first subview should follow the second
   *         Zero               - The two subviews are the same
   */
  public int compareIDs( IShellViewCtx subview1, IShellViewCtx subview2 ) {
    return 0;
  }

  /**
   * Creates an IShellView object.  Note the object created must be different
   * than this view i.e., different references, because the Integrator may
   * instruct this view to create more than one independent view.
   *
   * @return an IShellView object representing this view
   */
  public IShellView createView(Object aObject) {
    return new FolderView(fFolder.getFolder());
  }

  /**
   * Returns the view's preferences that display in the shell's shared
   * preference window.
   *
   * @return an object specifying property information for the view's
   * preferences.  Note the property information is found via introspection.
   * @see java.beans.BeanInfo
   */
  public Object getGlobalPreferences() {
    return null;
  }

  /**
   * Returns attributes of this view.
   *
   * @return one or more flags describing the specified subview's attributes
   * @see #CANCOPY
   * @see #CANDELETE
   * @see #CANMOVE
   * @see #CANRENAME
   * @see #READONLY
   * @see #HASSUBFOLDER
   * @see #FOLDER
   */
  public int getAttributes() {
   return FOLDER|READONLY|HASSUBFOLDER;
  }

  /**
   * Supplies an icon for this view
   *
   * @param iTypeFlags one or more flags specifying the requested icon's type
   * @see java.beans.BeanInfo#ICON_COLOR_16x16
   * @see java.beans.BeanInfo#ICON_MONO_16x16
   * @see java.beans.BeanInfo#ICON_COLOR_32x32
   * @see java.beans.BeanInfo#ICON_MONO_32x32
   * @see #OPEN
   *
   * @return an icon for the subview
   */
  public Image getIcon( int iTypeFlags ) {
    Image res = null;

    switch (iTypeFlags) {
    case java.beans.BeanInfo.ICON_COLOR_16x16:
    case java.beans.BeanInfo.ICON_MONO_16x16:
      res = UIFactory.Instance().getFolderImage(fFolder,
                                                false, false);
      break;

    case java.beans.BeanInfo.ICON_COLOR_32x32:
    case java.beans.BeanInfo.ICON_MONO_32x32:
      res = UIFactory.Instance().getFolderImage(fFolder,
                                                false, true);
      break;
    }

    return res;
  }

  /**
   * Returns a human readable name for this view
   *
   * @return a string representation of this view
   */
  public String getDisplayName() {
    return fFolder.getFolder().getName();
  }

  /**
   * Sets the display name for the specified subview
   *
   * @param name the new display name for this view
   */
  public void setDisplayName( String name ) {
  }

  /**
   * Adds a change listener for monitoring changes on the ctx.
   * e.g., the view ctx child state may have changed.
   */
  public void addShellViewCtxListener( ShellViewCtxListener l ) {
    fListeners.add(ShellViewCtxListener.class, l);
  }

  /**
   * Removes a change listener for monitoring changes on the ctx.
   */
  public void removeShellViewCtxListener( ShellViewCtxListener l ) {
    fListeners.remove(ShellViewCtxListener.class, l);
  }

  public void setDataHandler(DataHandler aHandler) {
    fDataHandler = aHandler;
  }

  public DataHandler getDataHandler() {
    return fDataHandler;
  }

  void initChildren() {
    fChildren.removeAllElements();
    ViewedFolder folder = fFolder.getFirstSubFolder();
    while (folder != null) {
      FolderCtx child = new FolderCtx(folder);
      fChildren.addElement(child);
      child.initialize(fIntegrator, this);
      folder = folder.getNextFolder();
    }
    fInited = true;
    notifyChange();
  }

  void notifyChange() {
    Object[] listeners = fListeners.getListenerList();
    ChangeEvent event = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      // Lazily create the event:
      if (event == null)
        event = new ChangeEvent(this);
      ((ShellViewCtxListener)listeners[i+1]).childrenChanged(event);
    }
  }
}
