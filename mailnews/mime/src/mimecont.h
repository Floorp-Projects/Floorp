/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  PRInt32 nchildren;			/* how many */
};

#endif /* _MIMECONT_H_ */
