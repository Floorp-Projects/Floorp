//
//  TestingPref.h
//  Testing
//
//  Created by Simon Fraser on Wed Jun 19 2002.
//  Copyright (c) 2000 __MyCompanyName__. All rights reserved.
//

#import <PreferencePaneBase.h>

@interface OrgMozillaChimeraPreferenceAppearance : PreferencePaneBase 
{
  IBOutlet NSButton 	*checkboxUnderlineLinks;
  IBOutlet NSButton 	*checkboxUseMyColors;
  IBOutlet NSColorWell *colorwellBackgroundColor;
  IBOutlet NSColorWell *colorwellTextColor;
  IBOutlet NSColorWell *colorwellUnvisitedLinks;
  IBOutlet NSColorWell *colorwellVisitedLinks;  
}

- (void) mainViewDidLoad;

- (IBAction)buttonClicked:(id)sender; 
- (IBAction)colorChanged:(id)sender;

@end
