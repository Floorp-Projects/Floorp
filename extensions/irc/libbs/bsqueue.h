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

#ifndef bsqueue_h___
#define bsqueue_h___

#include "prtypes.h"
#include "bspubtd.h"

PR_BEGIN_EXTERN_C

typedef PRUint32 cron_t;

BSQueueClass *
bs_queue_new (void);

void
bs_queue_free (BSQueueClass *queue);

int
bs_queue_add (BSQueueClass *queue, void *data, cron_t cron);

void *
bs_queue_pop (BSQueueClass *queue, cron_t *cron);

void *
bs_queue_peek (BSQueueClass *queue, cron_t *cron);

int
bs_queue_size (BSQueueClass *queue);

PR_END_EXTERN_C

#endif /* bsqueue_h___ */
