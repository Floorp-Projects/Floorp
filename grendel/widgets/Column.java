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
 * Created: Will Scullin <scullin@netscape.com>, 21 Aug 1997.
 */

package grendel.widgets;

import java.util.Vector;

import javax.swing.Icon;
import javax.swing.event.ChangeEvent;

/**
 * A Column is a vertical display unit for a table that maintains width, rendering,
 * and header display data. A column has no data model, thus it is only useful
 * in conjunction with another UI component that provides a one.
 */

public class Column {
  Icon     fIcon;
  String   fTitle;
  Object   fID;
  int      fWidth, fMinWidth, fMaxWidth;
  boolean  fResizeable;
  boolean  fSelectable;
  Vector   fListeners = new Vector();

  CellRenderer   fCellRenderer = new DefaultCellRenderer();
  CellEditor     fCellEditor;
  HeaderRenderer fHeaderRenderer = new DefaultHeaderRenderer();

  /**
   * Constructs a column with the given ID and title
   */

  public Column(Object aID, String aTitle) {
    this(aID);
    fTitle = new String(aTitle);
  }

  /**
   * Contructs a column with the given ID and icon
   */

  public Column(Object aID, Icon aIcon) {
    this(aID);
    fIcon = aIcon;
  }

  /**
   * Constructs a column with the given ID
   */

  public Column(Object aID) {
    fID = aID;
    fMinWidth = 10;
    fWidth = 100;
    fMaxWidth = 1000;
    fResizeable = true;
  }

  /**
   * Sets the width of the column.
   * @see getWidth()
   */

  public void setWidth(int aWidth) {
    if (fWidth != aWidth) {
      fWidth = aWidth;

      notifyWidthChange();
    }
  }

  /**
   * Returns the current column width
   * @see setWidth()
   */

  public int getWidth() {
    return fWidth;
  }

  /**
   * Sets the minimum column width. This width
   * is for reference only, it is not enforced
   * @see getMinWidth()
   */

  public void setMinWidth(int aWidth) {
    fMinWidth = aWidth;
  }

  /**
   * Returns the minimum column width. This width
   * is for reference only, it is not enforced
   * @see setMinWidth()
   */

  public int getMinWidth() {
    return fMinWidth;
  }

  /**
   * Sets the maximum column width. This width
   * is for reference only, it is not enforced
   * @see getMaxWidth()
   */

  public void setMaxWidth(int aWidth) {
    fMaxWidth = aWidth;
  }

  /**
   * returns the maximum column width. This width
   * is for reference only, it is not enforced
   * @see setMaxWidth()
   */

  public int getMaxWidth() {
    return fMaxWidth;
  }

  /**
   * Sets whether or not the column is resizable. This
   * attribute is for reference only, it is not enforced.
   * @see isResizeable()
   */

  public void setResizeable(boolean aResizeable) {
    fResizeable = aResizeable;
  }

  /**
   * Returns whether or not the column is resizable. This
   * attribute is for reference only, it is not enforced.
   * @see setResizeable()
   */

  public boolean isResizeable() {
    return fResizeable;
  }

  /**
   * Sets whether or not the column is selectable.
   * @see isSelectable()
   */

  public void setSelectable(boolean aSelectable) {
    fSelectable = aSelectable;
  }

  /**
   * Returns whether or not the column is resizable. This
   * attribute is for reference only, it is not enforced.
   * @see setSelectable()
   */

  public boolean isSelectable() {
    return fSelectable;
  }

  /**
   * Sets the column title. This is intended to be displayed
   * in the header above the column.
   * @see getTitle()
   */

  public void setTitle(String aTitle) {
    fTitle = new String(aTitle);
  }

  /**
   * Returns the column title.
   * @see setTitle()
   */

  public String getTitle() {
    return fTitle;
  }

  /**
   * Sets the icon for the column. This is intended to be
   * displayed in the header above the column
   *
   * @see getIcon()
   */

  public void setIcon(Icon aIcon) {
    fIcon = aIcon;
  }

  /**
   * Returns the column icon
   *
   * @see setIcon()
   */

  public Icon getIcon() {
    return fIcon;
  }

  /**
   * Sets the column's user defined id. The user can define
   * whatever value they find meaningful for this purpose.
   * @see getID()
   */

  public void setID(Object aID) {
    fID = aID;
  }

  /**
   * Returns the column's user defined id.
   * @see setID()
   */

  public Object getID() {
    return fID;
  }

  /**
   * Sets the cell renderer for this column. This renderer
   * is used for column data provided by another source.
   * @see getCellRenderer()
   * @see CellRenderer
   */

  public void setCellRenderer(CellRenderer aRenderer) {
    fCellRenderer = aRenderer;
  }

  /**
   * Returns the cell renderer for this column.
   * @see setCellRenderer()
   * @see CellRenderer
   */

  public CellRenderer getCellRenderer() {
    return fCellRenderer;
  }

  /**
   * Not implemented
   */

  public void setCellEditor(CellEditor aEditor) {
    fCellEditor = aEditor;
  }

  /**
   * Not implemented
   */

  public CellEditor getCellEditor() {
    return fCellEditor;
  }

  /**
   * Sets the column's header renderer. This renders the
   * column caption in a space provided by that object
   * @see getHeaderRenderer()
   * @see javax.swing.CellRenderer
   */

  public void setHeaderRenderer(HeaderRenderer aRenderer) {
    fHeaderRenderer = aRenderer;
  }

  /**
   * Returns the column's header renderer
   * @see setHeaderRenderer()
   */

  public HeaderRenderer getHeaderRenderer() {
    return fHeaderRenderer;
  }

  /**
   * Adds a ColumnChangeListener. The listener is notified whenever
   * a significant attribute changes. Currently this only includes
   * the column width, but this will expand.
   * @see removeColumnListener()
   * @see ColumnChangeListener
   */

  public void addColumnChangeListener(ColumnChangeListener aListener) {
    fListeners.addElement(aListener);
  }

  /**
   * Removes a ColumnChangeListener.
   * @see addColumnChangeListener.
   */

  public void removeColumnChangeListener(ColumnChangeListener aListener) {
    fListeners.removeElement(aListener);
  }

  /**
   * Used internally
   */

  void notifyWidthChange() {
    ChangeEvent event = new ChangeEvent(this);
    for (int i = 0; i < fListeners.size(); i++) {
      ColumnChangeListener listener =
        (ColumnChangeListener) fListeners.elementAt(i);
      listener.columnWidthChanged(event);
    }
  }
}
