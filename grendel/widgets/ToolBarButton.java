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
 * Corporation.  Portions created by Netscape are Copyright
 * (C) 1997 Netscape Communications Corporation.  All Rights Reserved.
 *
 * Contributed by: Jeff Galyan <talisman@anamorphic.com>
 */

package grendel.widgets;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

/** The ToolBarButton class implements the standard Netscape toolbar button
 *  behaviour of painting a flat border normally, and a raised border when
 *  the mouse moves over it. 
 *
 *  @author <a href="mailto:talisman@anamorphic.com">Jeff Galyan</a>
 *  @version $Revision: 1.1 $
 */
public class ToolBarButton extends JButton implements MouseListener {

  // Constructors
  /** Default constructor.
   */
  public ToolBarButton() { 
    super();
    addMouseListener(this);
  }

  /** Creates a button with an icon only.
   *  @param icon The Icon object to paint on the button.
   */
  public ToolBarButton( javax.swing.Icon icon ) { 
    super(icon);
    addMouseListener(this);
  }

  /** Creates a button with a text label only.
   *  @param text The text for the label.
   */
  public ToolBarButton( String text ) { 
    super(text);
    addMouseListener(this);
  }

  /** Creates a button with a text label and an icon.
   *  @param text The label text.
   *  @param icon The icon.
   */
  public ToolBarButton( String text, javax.swing.Icon icon ) { 
    super(text, icon);
    addMouseListener(this);
  }
  
  // MouseListener implementation. The only events the button itself cares
  // about are mouseEntered and mouseExited, but we have to provide no-op
  // implementations of the rest to satisfy the implements clause.

  /** Paints a raised border when the mouse passes over the button.
   *  @param evt The MouseEvent which triggered this handler.
   */
  public void mouseEntered(MouseEvent evt) {
    setBorder(BorderFactory.createRaisedBevelBorder());
  }

  /** Paints an empty border when the mouse leaves the button area.
   *  @param evt The MouseEvent which triggered this handler.
   */
  public void mouseExited(MouseEvent evt) {
    setBorder(BorderFactory.createEmptyBorder());
  }

  // No-ops to satisfy the implements clause of this class.

  public void mouseClicked(MouseEvent evt) { }
  public void mousePressed(MouseEvent evt) { }
  public void mouseReleased(MouseEvent evt) { }
}
