/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __MAI_INTERFACE_ACTION_H__
#define __MAI_INTERFACE_ACTION_H__

#include "nsMaiInterface.h"

class MaiInterfaceAction: public MaiInterface
{
public:
    MaiInterfaceAction(nsAccessibleWrap*);
    virtual ~MaiInterfaceAction();

    virtual MaiInterfaceType GetType();
    virtual const GInterfaceInfo *GetInterfaceInfo();

    const char *GetName() {
        return mName.get();
    }
    void SetName(nsAString &aString) { mName = NS_ConvertUCS2toUTF8(aString); }

    const char *GetKeyBinding() {
        return NS_ConvertUCS2toUTF8(mKeyBinding).get(); 
    }
    void SetKeyBinding(nsAString &aString) { mKeyBinding = aString; }
private:
    nsCString mName;
    nsString mKeyBinding;
};

#endif /* __MAI_INTERFACE_ACTION_H__ */
