/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

 * Contributor(s): Daniel Park <daepark@apmindsf.com>
 *
 */

package org.mozilla.webclient.impl.wrapper_native;

import org.w3c.dom.Node;

import org.mozilla.webclient.Selection;

public class SelectionImpl extends Object implements Selection {

  private String _selection = null;
  private Node _startContainer = null;
  private Node _endContainer = null;
  private int _startOffset = -1;
  private int _endOffset = -1;

  /**
   * Create an invalid (or uninitialized) Selection object.
   */
  public SelectionImpl() {}

  public void init(String selection,
                   Node startContainer,
                   Node endContainer,
                   int startOffset,
                   int endOffset)
  {
    _selection = selection;
    _startContainer = startContainer;
    _endContainer = endContainer;
    _startOffset = startOffset;
    _endOffset = endOffset;
  }

  /**
   * Get the text representation of this Selection object.
   */
  public String toString() {
    return (_selection);
  }

  /**
   * Get the Node that contains the start of this selection.
   */
  public Node getStartContainer() {
    return (_startContainer);
  }

  /**
   * Get the Node that contains the end of this selection.
   */
  public Node getEndContainer() {
    return (_endContainer);
  }

  /**
   * Get the offset to which the selection starts within the startContainer.
   */
  public int getStartOffset() {
    return (_startOffset);
  }

  /**
   * Get the offset to which the selection ends within the endContainer.
   */
  public int getEndOffset() {
    return (_endOffset);
  }

  /**
   * Test if the selection properties have been set.
   */
  public boolean isValid() {
    if (_selection == null ||
        _startContainer == null ||
        _endContainer == null ||
        _startOffset == -1 ||
        _endOffset == -1) {
      return (false);
    }

    return (true);
  }

} // end class "SelectionImpl"
