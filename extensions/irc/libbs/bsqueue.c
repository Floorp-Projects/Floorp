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

#include "prtypes.h"
#include "prmem.h"
#include "prlog.h"

#include "bsqueue.h"
#include "bspubtd.h"

typedef int QueueCompareOp (void *, void *);

typedef struct BSQueueElementClass BSQueueElementClass;

struct BSQueueElementClass {
    void *data;
    cron_t cron; /* elements will be sorted in cron order */
    BSQueueElementClass *up;
};

struct BSQueueClass {
    bsuint size;
    BSQueueElementClass *bottom;  /* queue pulls from the bottom up */
    QueueCompareOp *CompareOp;
};

BSQueueClass*
bs_queue_new (void)
{
    BSQueueClass *newqueue;

    newqueue = PR_NEW (BSQueueClass);

    newqueue->bottom = NULL;
    newqueue->size = 0;

    return newqueue;
    
}

void
bs_queue_free (BSQueueClass *queue)
{

    PR_ASSERT (queue->size == 0);
    PR_ASSERT (queue->bottom == NULL);

    PR_Free (queue);

    queue = NULL;
    
}

int
bs_queue_add (BSQueueClass *queue, void *data, cron_t cron)
{
    BSQueueElementClass *newelement, *currentelement, *previouselement = NULL;

    PR_ASSERT (queue);
    
    newelement = PR_NEW (BSQueueElementClass);

    if (!newelement)
        return 0;
    
    newelement->up = NULL;
    newelement->data = data;
    newelement->cron = cron;

    if (queue->bottom == NULL) /* if the queue is empty */
    {
        queue->bottom = newelement; /* then this is the first element */
        queue->size = 1;
    }
    else /* otherwise */
    {
        currentelement = queue->bottom;  /* search for the right spot */

	/* find the first element with a smaller cron than this element */
        while (currentelement && (newelement->cron >= currentelement->cron))
        {
            previouselement = currentelement;
            currentelement = currentelement->up;
        }        

        if (currentelement) /* if we're not left at the top of the queue */
        {
	    if (previouselement) /* were either in the middle, */
            {
                newelement->up = currentelement;  /* insert the new element */
                previouselement->up = newelement; /* above the current one  */
            }
	    else /* or at the bottom */
            {
                newelement->up = queue->bottom;
                queue->bottom = newelement;
            }
            
        }
        else /* otherwise */
        {
            previouselement->up = newelement; /* this is the new top element */
        }

        queue->size++;
        
    }

    return queue->size;
    
}

void *
bs_queue_pop (BSQueueClass *queue, cron_t *cron)
{
    BSQueueElementClass *bottomelement;
    void *rval;

    PR_ASSERT (queue);
    
    if (queue->size < 1)
        return NULL;

    bottomelement = queue->bottom;

    if (cron)
        *cron = queue->bottom->cron;
    
    rval = bottomelement->data;

    queue->bottom = bottomelement->up;

    queue->size--;
    
    if (queue->size < 1)
    {
        queue->bottom = NULL;
    }
    
    PR_Free (bottomelement);
    
    return rval;
    
}

void *
bs_queue_peek (BSQueueClass *queue, cron_t *cron)
{

    PR_ASSERT (queue);

    if (queue->size < 1)
        return NULL;

    if (cron)
        *cron = queue->bottom->cron;

    return (queue->bottom->data);

}    

int
bs_queue_size (BSQueueClass *queue)
{
    PR_ASSERT (queue);

    return queue->size;
    
}

/* deletes all queue items which have the data *data */
int
bs_queue_delete (BSQueueClass *queue, void *data)
{
    BSQueueElementClass *currentelement, *lastelement;

    if (!(currentelement = queue->bottom))
        return 0;

    /* move the bottom up */
    while (queue->bottom->data == data)
    {
        lastelement = queue->bottom;
        queue->bottom = queue->bottom->up;
        queue->size--;
        
        PR_Free (lastelement);

        currentelement = queue->bottom;
    }

    /* then trim the middle */
    while (currentelement->up)
    {
        if (currentelement->up->data == data)
        {
            currentelement->up = currentelement->up->up;
            lastelement = currentelement->up;
            queue->size--;
            
            PR_Free (lastelement);
        }

        currentelement = currentelement->up;
    }

    return 1;
    
}

            
        
    
