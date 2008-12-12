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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#ifndef nsHtml5ArrayCopy_h__
#define nsHtml5ArrayCopy_h__

#include "prtypes.h"

class nsString;
class nsHtml5StackNode;
class nsHtml5AttributeName;

// Unfortunately, these don't work as template functions because the arguments
// would need coercion from a template class, which complicates things.

class nsHtml5ArrayCopy {
  public: 
    static
    inline
    void
    arraycopy(PRUnichar* source, PRInt32 sourceOffset, PRUnichar* target, PRInt32 targetOffset, PRInt32 length)
    {
      memcpy(&(target[targetOffset]), &(source[sourceOffset]), length * sizeof(PRUnichar));
    }
    
    static
    inline
    void
    arraycopy(PRUnichar* source, PRUnichar* target, PRInt32 length)
    {
      memcpy(target, source, length * sizeof(PRUnichar));
    }
    
    static
    inline
    void
    arraycopy(nsString** source, nsString** target, PRInt32 length)
    {
      memcpy(target, source, length * sizeof(nsString*));
    }
    
    static
    inline
    void
    arraycopy(nsHtml5AttributeName** source, nsHtml5AttributeName** target, PRInt32 length)
    {
      memcpy(target, source, length * sizeof(nsHtml5AttributeName*));
    }
    
    static
    inline
    void
    arraycopy(nsHtml5StackNode** source, nsHtml5StackNode** target, PRInt32 length)
    {
      memcpy(target, source, length * sizeof(nsHtml5StackNode*));
    }
    
    static
    inline
    void
    arraycopy(nsHtml5StackNode** arr, PRInt32 sourceOffset, PRInt32 targetOffset, PRInt32 length)
    {
      memmove(&(arr[targetOffset]), &(arr[sourceOffset]), length * sizeof(nsHtml5StackNode*));
    }
  
};

#endif // nsHtml5ArrayCopy_h__
