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

#ifndef bsconfig_h___
#define bsconfig_h___

PR_BEGIN_EXTERN_C

#define BS_CONNECTION_TIMEOUT_MS  30 * 1000  /* timeout for connect()s */
#define BS_SEND_TIMEOUT_MS        30 * 1000  /* timeout for send()s */
#define BS_RECV_TIMEOUT_MS                1  /* timeout for recv()s */

/* property defaults */

#define BS_EVENTS_PER_STEP_DEFAULT  3
#define BS_INPUT_TIMEOUT_DEFAULT    2000
#define BS_LINEBUFFER_FLAG_DEFAULT  PR_TRUE
#define BSMAX_SERVER_RECV           4095

PR_END_EXTERN_C

#endif /* bsconfig_h___ */
