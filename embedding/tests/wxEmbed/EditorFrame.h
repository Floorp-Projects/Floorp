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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
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

#ifndef EDITORFRAME_H
#define EDITORFRAME_H

#include "GeckoFrame.h"

#include "nsICommandManager.h"
#include "nsICommandParams.h"

class EditorFrame :
    public GeckoFrame
{
protected:
    DECLARE_EVENT_TABLE()
    void OnEditBold(wxCommandEvent &event);
    void OnEditItalic(wxCommandEvent &event);
    void OnEditUnderline(wxCommandEvent &event);
    void OnEditIndent(wxCommandEvent &event);
    void OnEditOutdent(wxCommandEvent &event);
    void OnUpdateToggleCmd(wxUpdateUIEvent &event);
    void OnEditAlignLeft(wxCommandEvent &event);
    void OnEditAlignRight(wxCommandEvent &event);
    void OnEditAlignCenter(wxCommandEvent &event);
public :
    EditorFrame(wxWindow* aParent);

    nsCOMPtr<nsICommandManager> mCommandManager;

    // GeckoContainerUI overrides
    virtual void UpdateStatusBarText(const PRUnichar* aStatusText);

    void MakeEditable();
    nsresult DoCommand(const char *aCommand, nsICommandParams *aCommandParams);
    void IsCommandEnabled(const char *aCommand, PRBool *retval);
    void GetCommandState(const char *aCommand, nsICommandParams *aCommandParams);
    nsresult ExecuteAttribParam(const char *aCommand, const char *aAttribute);
};

#endif