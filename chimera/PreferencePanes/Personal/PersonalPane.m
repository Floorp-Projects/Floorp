#import "PersonalPane.h"

@implementation PersonalPane


- (id) initWithBundle:(NSBundle *) bundle {
    self = [super initWithBundle:bundle] ;
    return self ;
}

- (void)awakeFromNib
{
  NSLog(@"Personal Pane awoke from nib");
}

- (void)mainViewDidLoad
{
  NSLog(@"Personal Pane did load main view");
}

@end
