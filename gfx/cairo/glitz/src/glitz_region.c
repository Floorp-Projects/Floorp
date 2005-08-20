/*
 * Copyright © 2004 David Reveman
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitzint.h"

#define REGION_ALLOC_CHUNK 16

#define BOX_SUBSUMS_BOX(b1, b2) \
  ((b2)->x1 >= (b1)->x1 && \
   (b2)->x2 <= (b1)->x2 && \
   (b2)->y1 >= (b1)->y1 && \
   (b2)->y2 <= (b1)->y2)

#define BOX_INTERSECTS_BOX(b1, b2) \
  ((b1)->x1 < (b2)->x2 && \
   (b1)->x2 > (b2)->x1 && \
   (b1)->y1 < (b2)->y2 && \
   (b1)->y2 > (b2)->y1)

#define BOX_CLOSE_TO_BOX(b1, b2) \
  ((b1)->x1 < ((b2)->x2 + 1) && \
   (b1)->x2 > ((b2)->x1 - 1) && \
   (b1)->y1 < ((b2)->y2 + 1) && \
   (b1)->y2 > ((b2)->y1 - 1))

#define BOX_NEXT_TO_BOX(b1, b2) \
  ((((b1)->x1 == (b2)->x2 || \
	(b1)->x2 == (b2)->x1) && \
    (b1)->y1 == (b2)->y1 && \
    (b1)->y2 == (b2)->y2) || \
   (((b1)->y1 == (b2)->y2 || \
     (b1)->y2 == (b2)->y1) && \
    (b1)->x1 == (b2)->x1 && \
    (b1)->x2 == (b2)->x2))

#define MERGE_BOXES(d, b1, b2) \
  { \
    (d)->x1 = MIN ((b1)->x1, (b2)->x1); \
    (d)->y1 = MIN ((b1)->y1, (b2)->y1); \
    (d)->x2 = MAX ((b1)->x2, (b2)->x2); \
    (d)->y2 = MAX ((b1)->y2, (b2)->y2); \
  }

/*
 * No real union, boxes that intersect are just joined into bigger boxes.
 * This is OK for our needs and it keeps the number of boxes down to a
 * minimum and makes it faster.
 */
glitz_status_t
glitz_region_union (glitz_region_t *region,
                    glitz_box_t    *ubox)
{ 
  if (region->n_box == 0) {
    region->extents = *ubox;
    region->box = &region->extents;
    region->n_box = 1;

    return GLITZ_STATUS_SUCCESS;
  }

  if (BOX_CLOSE_TO_BOX (ubox, &region->extents)) {
    glitz_box_t *box, *new_box, *dst_box;
    int         n_box;

    box = region->box;
    n_box = region->n_box;

    while (n_box--) {
      if (BOX_SUBSUMS_BOX (box, ubox))
        return GLITZ_STATUS_SUCCESS;
        
      box++;
    }
      
    box = region->box;
    n_box = region->n_box;

    new_box = ubox;
    dst_box = NULL;
    while (n_box--) {

      if (BOX_INTERSECTS_BOX (box, new_box) ||
          BOX_NEXT_TO_BOX (box, new_box)) {
	    
        if (dst_box) {
          /*
           * Remove box from region
           */
          region->n_box--;
          if (region->n_box == 1) {
            MERGE_BOXES (&region->extents, box, new_box);
            region->box = &region->extents;
            
            return GLITZ_STATUS_SUCCESS;
          } else {
            MERGE_BOXES (dst_box, box, new_box);
            if (n_box)
              memmove (box, box + 1, n_box * sizeof (glitz_box_t));
          }
          continue;
        } else {
          dst_box = box;
          MERGE_BOXES (dst_box, box, new_box);
          new_box = dst_box;
        }
      }
      box++;
    }
    
    if (dst_box) {
      if (region->n_box > 1)
        MERGE_BOXES (&region->extents, &region->extents, ubox);
      
      return GLITZ_STATUS_SUCCESS;
    }
  }

  /*
   * Add box to region
   */
  if (region->size < (region->n_box + 1)) {
    region->size += REGION_ALLOC_CHUNK;
    region->data = (void *) realloc (region->data,
				     sizeof (glitz_box_t) * region->size);
    if (!region->data)
      return GLITZ_STATUS_NO_MEMORY;
  }

  region->box = (glitz_box_t *) region->data;
  
  region->box[region->n_box] = *ubox;
  if (region->n_box == 1)
    region->box[0] = region->extents;

  region->n_box++;

  MERGE_BOXES (&region->extents, &region->extents, ubox);
  
  return GLITZ_STATUS_SUCCESS;
}
