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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 */

package org.mozilla.webclient;

import org.w3c.dom.Node;

 /**
 * <b>Selection</b> is more like a DOM Range object which represents a
 * portion of a document or a document fragment. This is described as
 * all of the content between two boundary points, the start and end
 * containers. A selection is define by the following properties:
 * startContainer, endContainer, startOffset, endOffset and the text
 * representation of the selection.
 *
 * <p>
 * <ul>
 *
 * <li><b>startContainer</b>: This property is a reference to a Node
 * within the document or document fragment.  The start of the range
 * is contained within this node and the boundary point is determined
 * by the startOffset property.
 *
 * <li><b>endContainer</b>: This property is a reference to a Node within the
 * document or document fragment.  The end of the range is contained
 * within this node and the boundary point is determined by the
 * endOffset property.
 *
 * <li><b>startOffset</b>: This property has one of two meanings, depending
 * upon the type of node that the startContainer references.  If the
 * startContainer is a Text node, Comment node or a CDATASection node
 * then this offset represents the number of characters from the
 * beginning of the node to where the boundary point lies.  Any other
 * type of node and it represents the child node index number that the
 * boundary point lies BEFORE.  If the offset is equal to the number
 * of child nodes then the boundary point lies after the last child
 * node.
 *
 * <li><b>endOffset</b>: This property has one of two meanings, depending
 * upon the type of node that the endContainer references.  If the
 * endContainer is a Text node, Comment node or a CDATASection node
 * then this offset represents the number of characters from the
 * beginning of the node to where the boundary point lies.  Any other
 * type of node and it represents the child node index number that the
 * boundary point lies BEFORE.  If the offset is equal to the number
 * of child nodes then the boundary point lies after the last child
 * node.
 *
 * </ul>
 *
 * @author daepark@apmindsf.com
 */

public interface Selection {

  /**
   * Initialize this Selection object.
   *
   * @param selection the text representation of this selection
   * @param startContainer the start of this selection is contained
   * within this node
   * @param endContainer the end of this selection is contained within
   * this node
   * @param startOffset the offset to which the selection starts
   * within the startContainer
   * @param endOffset the offset to which the selection ends within
   * the endContainer
   * @see #isValid
   */
  public void init(String selection,
                   Node startContainer,
                   Node endContainer,
                   int startOffset,
                   int endOffset);

  /**
   * Get the text representation of this Selection object.
   */
  public String toString();

  /**
   * Get the Node that contains the start of this selection.
   */
  public Node getStartContainer();

  /**
   * Get the Node that contains the end of this selection.
   */
  public Node getEndContainer();

  /**
   * Get the offset to which the selection starts within the startContainer.
   */
  public int getStartOffset();

  /**
   * Get the offset to which the selection ends within the endContainer.
   */
  public int getEndOffset();

  /**
   * Test if the selection properties have been set.
   */
  public boolean isValid();

} // end interface "Selection"
