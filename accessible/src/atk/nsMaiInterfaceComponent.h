/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
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

#ifndef __MAI_INTERFACE_COMPONENT_H__
#define __MAI_INTERFACE_COMPONENT_H__

#include "nsMai.h"

G_BEGIN_DECLS

/* component interface callbacks */
void componentInterfaceInitCB(AtkComponentIface *aIface);
AtkObject *refAccessibleAtPointCB(AtkComponent *aComponent,
                                  gint aAccX, gint aAccY,
                                  AtkCoordType aCoordType);
void getExtentsCB(AtkComponent *aComponent,
                  gint *aAccX, gint *aAccY,
                  gint *aAccWidth, gint *aAccHeight,
                  AtkCoordType aCoordType);
/* the "contains", "get_position", "get_size" can take advantage of
 * "get_extents", there is no need to implement them now.
 */
gboolean grabFocusCB(AtkComponent *aComponent);

/* what are missing now for atk component */

/* ==================================================
 * add_focus_handler
 * remove_focus_handler
 * set_extents
 * set_position
 * set_size
 * get_layer
 * get_mdi_zorder
 * ==================================================
 */
G_END_DECLS

#endif /* __MAI_INTERFACE_COMPONENT_H__ */
