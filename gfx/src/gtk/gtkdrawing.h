/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _GTK_DRAWING_H_
#define _GTK_DRAWING_H_

#include <gdk/gdk.h>
#include <gtk/gtkstyle.h>
#include "prtypes.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
  PRPackedBool active;
  PRPackedBool focused;
  PRPackedBool inHover;
  PRPackedBool disabled;
  PRPackedBool isDefault;
  PRPackedBool canDefault;
} GtkWidgetState;

typedef struct {
  GtkWidgetState wState;
  PRPackedBool selected;
} GtkToggleButtonState;

void
moz_gtk_button_paint(GdkWindow* window, GtkStyle* style,
                     GdkRectangle* buttonRect, GdkRectangle* clipRect,
                     GtkWidgetState* buttonState, GtkReliefStyle relief);

void
moz_gtk_checkbox_paint(GdkWindow* window, GtkStyle* style,
                       GdkRectangle* boxRect, GdkRectangle* clipRect,
                       GtkToggleButtonState* aState, const char* detail);

void
moz_gtk_scrollbar_button_paint(GdkWindow* window, GtkStyle* style,
                               GdkRectangle* arrowRect, GdkRectangle* clipRect,
                               GtkWidgetState* state, GtkArrowType type);

void
moz_gtk_scrollbar_trough_paint(GdkWindow* window, GtkStyle* style,
                               GdkRectangle* troughRect, GdkRectangle* clipRect,
                               GtkWidgetState* state);

void
moz_gtk_scrollbar_thumb_paint(GdkWindow* window, GtkStyle* style,
                              GdkRectangle* thumbRect, GdkRectangle* clipRect,
                              GtkWidgetState* state);


void
moz_gtk_gripper_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                      GdkRectangle* clipRect, GtkWidgetState* state);

void
moz_gtk_entry_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                    GdkRectangle* clipRect, GtkWidgetState* state);

void
moz_gtk_dropdown_arrow_paint(GdkWindow* window, GtkStyle* style, GdkRectangle* rect,
                             GdkRectangle* clipRect, GtkWidgetState* state);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
