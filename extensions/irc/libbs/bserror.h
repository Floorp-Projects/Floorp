/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Basic Socket Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

#ifndef bserror_h___
#define bserror_h___

#include "prerror.h"
#include "bspubtd.h"

PR_BEGIN_EXTERN_C

/* error codes */
#define BSERR_UNDEFINED_ERROR     0
#define BSERR_OUT_OF_MEMORY       1
#define BSERR_PROPERTY_MISMATCH   2
#define BSERR_NO_SUCH_PARAM       3
#define BSERR_NO_SUCH_EVENT       4
#define BSERR_CONNECTION_TIMEOUT  5
#define BSERR_READ_ONLY           6
#define BSERR_INVALID_STATE       7
#define BSERR_MAP_SIZE            7

#define OUT_OF_MEMORY BS_ReportError (BSERR_OUT_OF_MEMORY);

PR_EXTERN (void)
BS_ReportError (bsint err);

PR_EXTERN (void)
BS_ReportPRError (PRErrorCode err);

PR_END_EXTERN_C

#endif /* bserror_h___ */
