/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
   motifdnd.h --- atoms and other stuff defined for use with the
                  new motif drag and drop.
   Created: Chris Toshok <toshok@netscape.com>, 15-Jun-96.
 */


#ifndef MOTIFDND_H
#define MOTIFDND_H

#include <Xm/VendorS.h>
#include <Xm/DragDrop.h>

/* Atoms used for drag/drop targets -- these are initialized in
   the DND_Initialize routine. */


extern Atom NS_DND_URL;
extern Atom NS_DND_BOOKMARK;
extern Atom NS_DND_ADDRESS;
extern Atom NS_DND_HTMLTEXT;
extern Atom NS_DND_FOLDER;
extern Atom NS_DND_MESSAGE;
extern Atom NS_DND_COLUMN;
extern Atom NS_DND_COMPOUNDTEXT;

/* initialize target atoms and new drag start actions */
void DND_initialize_atoms(Widget toplevel);

/* used for install the drag start action on the location icon */
void DND_install_location_drag(Widget location_label);

#endif /* MOTIFDND_H */
