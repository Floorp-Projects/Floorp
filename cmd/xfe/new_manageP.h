/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* 
   new_manageP.h --- defines a subclass of XmScrolledWindow
   Created: Eric Bina <ebina@netscape.com>, 17-Aug-94.
 */


#ifndef _FE_NEWMANAGEP_H_
#define _FE_NEWMANAGEP_H_

#include "new_manage.h"
#include <Xm/ManagerP.h>

typedef struct
{
  int frogs;
} NewManageClassPart;

typedef struct _NewManageClassRec
{
  CoreClassPart	core_class;
  CompositeClassPart		composite_class;
  ConstraintClassPart		constraint_class;
  XmManagerClassPart		manager_class;
  NewManageClassPart		newManage_class;
} NewManageClassRec;

extern NewManageClassRec newManageClassRec;

typedef struct 
{
  void *why;
  XtCallbackList                input_callback;
} NewManagePart;

typedef struct _NewManageRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    NewManagePart		newManage;
} NewManageRec;

#endif /* _FE_NEWMANAGEP_H_ */

