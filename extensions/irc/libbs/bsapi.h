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

#ifndef bsapi_h___
#define bsapi_h___

PR_BEGIN_EXTERN_C

/* common properties */
#define BSPROP_PARENT  100
#define BSPROP_NAME    101
#define BSPROP_KEY     102
#define BSPROP_STATUS  103

/* network properties */
#define BSPROP_EVENTS_PER_STEP  200

/* server properties */
#define BSPROP_HOSTNAME         300
#define BSPROP_PORT             301
#define BSPROP_INPUT_TIMEOUT    302
#define BSPROP_LINE_BUFFER      303

/* connection properties */

/* event properties */
#define BSPROP_OBJECT_TYPE      400
#define BSPROP_DESTOBJECT       401
#define BSPROP_HANDLER_ID       402
#define BSPROP_DATA             403

#define BS_CHECK_TYPE(type, mask) (mask & type)

PR_END_EXTERN_C

#endif /* bsapi_h___ */
