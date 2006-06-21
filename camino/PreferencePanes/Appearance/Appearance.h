/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <sfraser@netscape.com>
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

#import "PreferencePaneBase.h"

@interface OrgMozillaChimeraPreferenceAppearance : PreferencePaneBase 
{
  IBOutlet NSTabView*     mTabView;

  // colors tab
  IBOutlet NSColorWell*   mTextColorWell;
  IBOutlet NSColorWell*   mBackgroundColorWell;

  IBOutlet NSColorWell*   mVisitedLinksColorWell;  
  IBOutlet NSColorWell*   mUnvisitedLinksColorWell;

  IBOutlet NSButton*      mUnderlineLinksCheckbox;
  IBOutlet NSButton*      mUseMyColorsCheckbox;

  IBOutlet NSButton*      mColorsResetButton;

  // fonts tab
  IBOutlet NSButton*      mChooseProportionalFontButton;
  IBOutlet NSButton*      mChooseMonospaceFontButton;
  IBOutlet NSPopUpButton* mFontRegionPopup;

  IBOutlet NSTextField*   mFontSampleProportional;
  IBOutlet NSTextField*   mFontSampleMonospace;

  IBOutlet NSTextField*   mProportionalSampleLabel;
  IBOutlet NSTextField*   mProportionalSubLabel;

  IBOutlet NSButton*      mUseMyFontsCheckbox;

  IBOutlet NSButton*      mFontsResetButton;

  // advanced panel stuff
  IBOutlet NSPanel*       mAdvancedFontsDialog;
  IBOutlet NSPopUpButton* mSerifFontPopup;
  IBOutlet NSPopUpButton* mSansSerifFontPopup;
  IBOutlet NSPopUpButton* mCursiveFontPopup;
  IBOutlet NSPopUpButton* mFantasyFontPopup;

  IBOutlet NSPopUpButton* mMinFontSizePopup;
  IBOutlet NSTextField*   mAdvancedFontsLabel;

  IBOutlet NSMatrix*      mDefaultFontMatrix;
  
  NSArray*                mRegionMappingTable;
  
  NSButton*               mFontButtonForEditor;
  
  NSTextView*             mPropSampleFieldEditor;
  NSTextView*             mMonoSampleFieldEditor;
  
  BOOL                    mFontPanelWasVisible;
}

- (void)mainViewDidLoad;

- (IBAction)buttonClicked:(id)sender; 
- (IBAction)colorChanged:(id)sender;

- (IBAction)proportionalFontChoiceButtonClicked:(id)sender;
- (IBAction)useMyFontsButtonClicked:(id)sender;
- (IBAction)monospaceFontChoiceButtonClicked:(id)sender;
- (IBAction)fontRegionPopupClicked:(id)sender;

- (IBAction)showAdvancedFontsDialog:(id)sender;
- (IBAction)advancedFontsDone:(id)sender;

- (IBAction)resetColorsToDefaults:(id)sender;
- (IBAction)resetFontsToDefaults:(id)sender;

@end
