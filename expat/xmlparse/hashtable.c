/*
Copyright (c) 1999 Thai Open Source Software Center Ltd
See the file COPYING for copying permission.
*/

#include "xmldef.h"

#ifdef XML_UNICODE_WCHAR_T
#ifndef XML_UNICODE
#define XML_UNICODE
#endif
#endif

#include "hashtable.h"

#define INIT_SIZE 64

static
int keyeq(KEY s1, KEY s2)
{
  for (; *s1 == *s2; s1++, s2++)
    if (*s1 == 0)
      return 1;
  return 0;
}

static
unsigned long hash(KEY s)
{
  unsigned long h = 0;
  while (*s)
    h = (h << 5) + h + (unsigned char)*s++;
  return h;
}

NAMED *lookup(HASH_TABLE *table, KEY name, size_t createSize)
{
  size_t i;
  if (table->size == 0) {
    if (!createSize)
      return 0;
    table->v = calloc(INIT_SIZE, sizeof(NAMED *));
    if (!table->v)
      return 0;
    table->size = INIT_SIZE;
    table->usedLim = INIT_SIZE / 2;
    i = hash(name) & (table->size - 1);
  }
  else {
    unsigned long h = hash(name);
    for (i = h & (table->size - 1);
         table->v[i];
         i == 0 ? i = table->size - 1 : --i) {
      if (keyeq(name, table->v[i]->name))
	return table->v[i];
    }
    if (!createSize)
      return 0;
    if (table->used == table->usedLim) {
      /* check for overflow */
      size_t newSize = table->size * 2;
      NAMED **newV = calloc(newSize, sizeof(NAMED *));
      if (!newV)
	return 0;
      for (i = 0; i < table->size; i++)
	if (table->v[i]) {
	  size_t j;
	  for (j = hash(table->v[i]->name) & (newSize - 1);
	       newV[j];
	       j == 0 ? j = newSize - 1 : --j)
	    ;
	  newV[j] = table->v[i];
	}
      free(table->v);
      table->v = newV;
      table->size = newSize;
      table->usedLim = newSize/2;
      for (i = h & (table->size - 1);
	   table->v[i];
	   i == 0 ? i = table->size - 1 : --i)
	;
    }
  }
  table->v[i] = calloc(1, createSize);
  if (!table->v[i])
    return 0;
  table->v[i]->name = name;
  (table->used)++;
  return table->v[i];
}

void hashTableDestroy(HASH_TABLE *table)
{
  size_t i;
  for (i = 0; i < table->size; i++) {
    NAMED *p = table->v[i];
    if (p)
      free(p);
  }
  free(table->v);
}

void hashTableInit(HASH_TABLE *p)
{
  p->size = 0;
  p->usedLim = 0;
  p->used = 0;
  p->v = 0;
}

void hashTableIterInit(HASH_TABLE_ITER *iter, const HASH_TABLE *table)
{
  iter->p = table->v;
  iter->end = iter->p + table->size;
}

NAMED *hashTableIterNext(HASH_TABLE_ITER *iter)
{
  while (iter->p != iter->end) {
    NAMED *tem = *(iter->p)++;
    if (tem)
      return tem;
  }
  return 0;
}

