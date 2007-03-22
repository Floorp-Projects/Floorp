/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is PPEmbed code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
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

// PowerPlant Config
#define PP_Target_Carbon			1
#define PP_Suppress_Notes_221		1
#define PP_MenuUtils_Option			PP_MenuUtils_AppearanceOnly
#define PP_Uses_Aqua_MenuBar		1

// Carbon Headers
#include <Carbon/Carbon.h>

// PowerPlant Headers
#include <LAction.h>
#include <LUndoer.h>
#include <UTETextAction.h>
#include <UTEViewTextAction.h>

#include <LModelDirector.h>
#include <LModelObject.h>
#include <LModelProperty.h>
#include <UAppleEventsMgr.h>
#include <UExtractFromAEDesc.h>

#include <LArray.h>
#include <LArrayIterator.h>
#include <LComparator.h>
#include <LRunArray.h>
#include <LVariableArray.h>
#include <TArray.h>
#include <TArrayIterator.h>

#include <LApplication.h>
#include <LCommander.h>
#include <LDocApplication.h>
#include <LDocument.h>
#include <LSingleDoc.h>

#include <LAttachable.h>
#include <LAttachment.h>
#include <LBroadcaster.h>
#include <LDragAndDrop.h>
#include <LDragTask.h>
#include <LEventDispatcher.h>
#include <LListener.h>
#include <LPeriodical.h>
#include <LSharable.h>

#include <LDataStream.h>
#include <LFile.h>
#include <LFileStream.h>
#include <LHandleStream.h>
#include <LStream.h>

#include <LButton.h>
#include <LCaption.h>
#include <LCicnButton.h>
#include <LControl.h>
#include <LDialogBox.h>
#include <LEditField.h>
#include <LFocusBox.h>
#include <LGrafPortView.h>
#include <LListBox.h>
#include <LOffscreenView.h>
#include <LPane.h>
#include <LPicture.h>
#include <LPlaceHolder.h>
#include <LPrintout.h>
#include <LRadioGroupView.h>
#include <LScroller.h>
#include <LStdControl.h>
#include <LTabGroupView.h>
#include <LTableView.h>
#include <LTextEditView.h>
#include <LView.h>
#include <LWindow.h>
#include <UGWorld.h>
#include <UQuickTime.h>

#include <PP_Constants.h>
#include <PP_KeyCodes.h>
#include <PP_Macros.h>
#include <PP_Messages.h>
#include <PP_Prefix.h>
#include <PP_Resources.h>
#include <PP_Types.h>

#include <LClipboard.h>
#include <LFileTypeList.h>
#include <LMenu.h>
#include <LMenuBar.h>
#include <LRadioGroup.h>
#include <LString.h>
#include <LTabGroup.h>
#include <UDesktop.h>

#include <UAttachments.h>
#include <UCursor.h>
#include <UDebugging.h>
#include <UDrawingState.h>
#include <UDrawingUtils.h>
#include <UEnvironment.h>
#include <UException.h>
#include <UKeyFilters.h>
#include <UMemoryMgr.h>
#include <UModalDialogs.h>
#include <UPrinting.h>
#include <UReanimator.h>
#include <URegions.h>
#include <URegistrar.h>
#include <UScrap.h>
#include <UScreenPort.h>
#include <UTextEdit.h>
#include <UTextTraits.h>
#include <UWindows.h>

// Mozilla
#include "mozilla-config.h"

// Flags applicable to any build
#include "PPEmbedConfig.h"

// Flags for this particular build
#define POWERPLANT_IS_FRAMEWORK
