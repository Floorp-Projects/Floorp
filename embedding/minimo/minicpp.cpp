/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Minimo.
 *
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@meer.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * minicpp
 * ======================
 * 
 * Mozilla requires only a few libstdc++ functions.  Minicpp
 * is a simple stub that provides these functions allowing 
 * some application using Mozilla not to include libstdc++
 * with their distribution.
 * 
 * To build minicpp, on linux:
 * 
 * gcc -shared -o libminicpp.so -fPIC minicpp.cpp
 * 
 * Results may vary.  
 */

#include <stdlib.h>

//global new and delete replacements to avoid need for libstdc++

void* __builtin_new(size_t size){
   return (void*) malloc(size);
}

void* __builtin_vec_new(size_t size){
   return __builtin_new( size );
}

void* __builtin_delete(void* o){
   free(o);
}

void __builtin_vec_delete( void* o ){
   __builtin_delete( o );
}

void __terminate(void)
{ //abort or something??
} 

void* operator new(size_t size){
  return malloc(size);
}
     
void* operator new[](size_t size){
  return malloc(size);
}
  
void operator delete(void* o){
  return free(o);
}

void operator delete[](void* o){
  return free(o);
}
 
extern "C" void
__cxa_pure_virtual(void)
{
}    
