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
  IBOutlet NSTabView 		*tabView;

  IBOutlet NSButton 		*checkboxUnderlineLinks;
  IBOutlet NSButton 		*checkboxUseMyColors;
  IBOutlet NSColorWell 	*colorwellBackgroundColor;
  IBOutlet NSColorWell 	*colorwellTextColor;
  IBOutlet NSColorWell 	*colorwellUnvisitedLinks;
  IBOutlet NSColorWell 	*colorwellVisitedLinks;  

  IBOutlet NSMatrix		 	*matrixChooseFont;
  IBOutlet NSTableView 	*tableViewFontRegion;

  IBOutlet NSTextField	*fontSampleSerif;
  IBOutlet NSTextField	*fontSampleSansSerif;
  IBOutlet NSTextField	*fontSampleMonospace;
  IBOutlet NSTextField	*fontSampleCursive;

  NSArray								*regionMappingTable;
}

- (void) mainViewDidLoad;

- (IBAction)buttonClicked:(id)sender; 
- (IBAction)colorChanged:(id)sender;

- (IBAction)fontChoiceButtonClicked:(id)sender;
- (IBAction)fontRegionListClicked:(id)sender;


@end
