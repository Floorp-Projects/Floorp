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
 * Created: Will Scullin <scullin@netscape.com>, 21 Aug 1997.
 */

package grendel.widgets;

import java.util.Enumeration;

/**
 * An interface for the data model that stores the layout of a set of columns.
 * This interface is shared by the ColumnHeaders and the TreeTable to provide
 * a common column layout for both.
 */

public interface ColumnModel {

  /**
   * Add a column to the model. The column is added at the end.
   */

  public void addColumn(Column aColumn);

  /**
   * Add a column to the model at a specified index. An exception may be
   * if the index is invalid.
   */

  public void addColumnAt(Column aColumn, int aIndex);

  /**
   * Removes a column from the model.
   */

  public void removeColumn(Column aColumn);

  /**
   * Moves a column at the source index to the destination index. Columns
   * in between may be shifted. An exception may be thrown if either index
   * is invalid.
   */

  public void moveColumn(int aSource, int aDest);

  /**
   * @return The total number of columns in this model.
   */

  public int getColumnCount();

  /**
   * @return The column at the given index. Returns null if
   *         index is invalid.
   */

  public Column getColumn(int aIndex);

  /**
   * @return The column with the given ID. Returns null if a column with the
   *         given ID is not in the model.
   */

  public Column getColumn(Object aID);

  /**
   * @return An enumeration of all the columns in the model.
   */

  public Enumeration getColumns();

  /**
   * @return The index of the column in the model. Returns -1 if the column is
   *         not in the model.
   */

  public int getColumnIndex(Column aColumn);

  /**
   * @return The index of the column at the given X location. Returns -1 if the
   *         given X does not fall over a column.
   */

  public int getColumnIndexAtX(int aX);

  /**
   * @return The margin (gap) between columns.
   */

  public int getColumnMargin();

  /**
   * Sets the margin (gap) between columns.
   */

  public void setColumnMargin(int aMargin);

  /**
   * @return The sum of all column widths and margins.
   */

  public int getTotalColumnWidth();

  /**
   * Adds a ColumnModelListener.
   */

  public void addColumnModelListener(ColumnModelListener aListener);

  /**
   * Removes a ColumnModelListener.
   */

  public void removeColumnModelListener(ColumnModelListener aListener);


  /**
   * @return The currently selected column.
   */

  public Column getSelectedColumn();

  /**
   * Sets the currently selected column. Currently only single selection
   * is supported.
   */

  public void setSelectedColumn(Column aColumn);

  /**
   * @return A string representing the layout (order and widths) of the
   *         columns.
   */

  public String getPrefString();

  /**
   * Restores a column layout that was obtained from getPrefString().
   */

  public void setPrefString(String aString);
}
