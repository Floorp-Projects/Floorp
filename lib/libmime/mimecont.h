/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* mimecont.h --- definition of the MimeContainer class (see mimei.h)
   Created: Jamie Zawinski <jwz@netscape.com>, 15-May-96.
 */


#ifndef _MIMECONT_H_
#define _MIMECONT_H_

#include "mimeobj.h"

/* MimeContainer is the class for the objects representing all MIME 
   types which can contain other MIME objects within them.  In addition 
   to the methods inherited from MimeObject, it provides one method:

   int add_child (MimeObject *parent, MimeObject *child)

     Given a parent (a subclass of MimeContainer) this method adds the
     child (any MIME object) to the parent's list of children.

     The MimeContainer `finalize' method will finalize the children as well.
 */

typedef struct MimeContainerClass MimeContainerClass;
typedef struct MimeContainer      MimeContainer;

struct MimeContainerClass {
  MimeObjectClass object;
  int (*add_child) (MimeObject *parent, MimeObject *child);
};

extern MimeContainerClass mimeContainerClass;

struct MimeContainer {
  MimeObject object;		/* superclass variables */

  MimeObject **children;	/* list of contained objects */
  int32 nchildren;			/* how many */
};

#endif /* _MIMECONT_H_ */
