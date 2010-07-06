/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code are Kevin Hendricks (MySpell)
 * and László Németh (Hunspell). Portions created by the Initial Developers
 * are Copyright (C) 2002-2005 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s): László Németh (nemethl@gyorsposta.hu)
 *                 Caolan McNamara (caolanm@redhat.com)
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
 ******* END LICENSE BLOCK *******/

/* string replacement list class */
#ifndef _REPLIST_HXX_
#define _REPLIST_HXX_

#include "hunvisapi.h"

#include "w_char.hxx"

class LIBHUNSPELL_DLL_EXPORTED RepList
{
protected:
    replentry ** dat;
    int size;
    int pos;

public:
    RepList(int n);
    ~RepList();

    int get_pos();
    int add(char * pat1, char * pat2);
    replentry * item(int n);
    int near(const char * word);
    int match(const char * word, int n);
    int conv(const char * word, char * dest);
};
#endif
