#import <Foundation/Foundation.h>

bool GetDocumentsDirectory(char* dir)
{
    NSSearchPathDirectory directory = NSDocumentDirectory;
    NSArray* paths = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES);
    if ([paths count] == 0) {
        return false;
    }
    strcpy(dir, [[paths objectAtIndex:0] UTF8String]);
    return true;
}
