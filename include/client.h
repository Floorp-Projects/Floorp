/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * This file should be included after xp_mcom.h
 *
 * All definitions for intermodule communications in the Netscape
 * client should be contained in this file
 */

#ifndef _CLIENT_H_
#define _CLIENT_H_

#define NEW_FE_CONTEXT_FUNCS

/* include header files needed for prototypes/etc */

#include "xp_mcom.h"

#include "ntypes.h" /* typedefs for commonly used Netscape data structures */
#include "fe_proto.h" /* all the standard FE functions */
#include "proto.h" /* library functions */

/* global data structures */
#include "structs.h"
#include "merrors.h"

#endif /* _CLIENT_H_ */

