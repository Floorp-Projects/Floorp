//
//  Appearance pref pane for chimera
//  
//
//  Created by Simon Fraser on Wed Jun 19 2002.
//  Copyright (c) 2000 __MyCompanyName__. All rights reserved.
//

#import "Appearance.h"

#include "nsIServiceManager.h"
#include "nsIPrefBranch.h"
#include "nsIPref.h"
#include "nsIMemory.h"

@implementation OrgMozillaChimeraPreferenceAppearance

- (void) mainViewDidLoad
{
  BOOL gotPref;
  [checkboxUnderlineLinks setState:
      [self getBooleanPref:"browser.underline_anchors" withSuccess:&gotPref]];
  [checkboxUseMyColors setState:
      ![self getBooleanPref:"browser.display.use_document_colors" withSuccess:&gotPref]];

  // should save and restore this
  [[NSColorPanel sharedColorPanel] setContinuous:NO];
  
  [colorwellBackgroundColor setColor:[self getColorPref:"browser.display.background_color" withSuccess:&gotPref]];
  [colorwellTextColor setColor:[self getColorPref:"browser.display.foreground_color" withSuccess:&gotPref]];
  [colorwellUnvisitedLinks setColor:[self getColorPref:"browser.anchor_color" withSuccess:&gotPref]];
  [colorwellVisitedLinks setColor:[self getColorPref:"browser.visited_color" withSuccess:&gotPref]];
}


- (IBAction)buttonClicked:(id)sender
{
  if (sender == checkboxUnderlineLinks)
  {
    [self setPref:"browser.underline_anchors" toBoolean:[sender state]];
  }
  else if (sender == checkboxUseMyColors)
  {
    [self setPref:"browser.display.use_document_colors" toBoolean:![sender state]];
  }
}

- (IBAction)colorChanged:(id)sender
{
  const char* prefName = NULL;
  
  if (sender == colorwellBackgroundColor)
  	prefName = "browser.display.background_color";
  else if (sender == colorwellTextColor)
  	prefName = "browser.display.foreground_color";
  else if (sender == colorwellUnvisitedLinks)
  	prefName = "browser.anchor_color";
  else if (sender == colorwellVisitedLinks)
  	prefName = "browser.visited_color";

  if (prefName)
    [self setPref:prefName toColor:[sender color]];
}

@end
