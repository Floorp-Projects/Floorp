/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: XmHelper.c,v 1.2 2001/07/12 19:58:19 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
/* 
   Note: this tiny DLL overrides some symbols to allow libXm to be 
   dynamically loaded _after_ libXt.
   The only thing you should care about is proper alignment:
   "struct dummy_struct_t" should have the same binary layout as 
   VendorShellClassRec - i.e. use the same alignments as X uses.
   On Solaris libawt.so does explicit check of name of library 
   where vendorShellWidgetClass defined, so this DLL should be called smth like
   Helper.libXm.so.4. Stupid dog.....
   Dedicated to Lyola.
   Compile
   gcc -D_GNU_SOURCE -O2 -fPIC -shared XmHelper.c -o Helper.libXm.so.4  -ldl
   then 
   export LD_PRELOAD=<path_to_the_Helper.libXm.so.4>
   in script running Mozilla.
*/

struct dummy_struct_t
{
  void* reserved1;
  void* reserved2;
  /* typedef unsigned int    Cardinal; */
  unsigned int   len;
  char  data[1024];
};

struct dummy_struct_t* vendorShellWidgetClass = NULL;
struct dummy_struct_t  vendorShellClassRec =
{
  NULL, NULL, 0, ""
};

static int is_our(const char* filename)
{
  if (strstr(filename, "libawt.so") || strstr(filename, "libmawt.so") ||
      strstr(filename, "libawt_g.so") || strstr(filename, "libmawt_g.so"))
    return 1;
  return 0;
};

void *dlopen (const char *filename, int flag)
{
  typedef void* (*dlopen_t)(const char *filename, int flag);
  static dlopen_t real = (void*)0;
  void* result;
  struct dummy_struct_t* s = (void*)0;
  static int done = 0;
  int size_core, size_total;

  //fprintf(stderr, "opening %s\n", filename);
  if (!real)
    real = (dlopen_t)dlsym(RTLD_NEXT, "dlopen");
  result = (*real)(filename, flag);
  if (done || !filename || !result) 
    return result;
  /* hate long ifs */
  if (!is_our(filename) && vendorShellWidgetClass) 
    return result;
  s = (struct dummy_struct_t*)dlsym(result, "vendorShellClassRec");
  if (s) {
        size_core = s->len;
        size_total = size_core /* CoreClassPart */ + 
          5*sizeof(void*) /* CompositeClassPart */ + 
          sizeof(void*) /* ShellClassPart */ +
          sizeof(void*) /* WMShellClassPart */ +
          sizeof(void*) /* VendorShellClassPart */;
        if (size_total > sizeof(struct dummy_struct_t))
          {
            fprintf(stderr, "Cannot override vendorShellClassRec: %d > %d\n",
                    size_total, sizeof(struct dummy_struct_t));
          }
        else
          {
            if (is_our(filename))
              {
                memcpy(&vendorShellClassRec, s, size_total);
                vendorShellWidgetClass = &vendorShellClassRec;
                done = 1;
              } 
            else if (!vendorShellWidgetClass)
              {     
                memcpy(&vendorShellClassRec, s, size_total);
                vendorShellWidgetClass = &vendorShellClassRec;
              }
          }
  }
  /* 
     to clean up possible dlsym() errors. 
     dlopen() errors won't be lost, as it already succeed
  */
  dlerror();
  return result;
}

/* awfully dirty hack to allow JVM loading into browser on Solaris,
   see src/solaris/hpi/native_threads/src/threads_solaris.c in JDK tree
   for explanations. Need help from JDK people. */
#ifdef __sun
#include <thread.h>
ulong_t __gettsp(thread_t thr)
{
  typedef ulong_t (*__gettsp_t)(thread_t thr);
  static  __gettsp_t real = (void*)0;
  if (!real)
    real = (__gettsp_t)dlsym(RTLD_NEXT, " __gettsp");
  return (*real)(thr);
}
#endif








