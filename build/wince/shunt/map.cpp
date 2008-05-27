#include "map.h"
#include "stdlib.h"

//right now, I'm assuming this stucture won't be huge, so implmenting with a linked list
extern "C" {
#if 0
}
#endif


mapping* getMapping(const char* key)
{
  mapping* cur = head;
  while(cur != NULL){
    if(!strcmp(cur->key,key))
      return cur;
    cur = cur->next;
  }
  return NULL;
}
 
int map_put(const char* key,const char* val)
{
  mapping* map = getMapping(key);
  if(map){
    if(!((map > initial_map) && 
         (map < (initial_map + init_i))))
      free( map->value);
  }else{	
    map = (mapping*)malloc(sizeof(mapping));
    map->key = (char*)malloc((strlen(key)+1)*sizeof(char));
    strcpy(map->key,key);
    map->next = head;
    head = map;
  }
  map->value = (char*)malloc((strlen(val)+1)*sizeof(char));
  strcpy(map->value,val);
  return 0;
}

char*  map_get(const char* key)
{
  mapping* map = getMapping(key);
  if(map)
    return map->value;
  return NULL;
}


#if 0
{
#endif
} /* extern "C" */

