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
   new_manage.c --- defines a subclass of XmScrolledWindow
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

