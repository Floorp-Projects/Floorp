#import <Cocoa/Cocoa.h>

#include "nsEmbedAPI.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsString.h"

int main(int argc, const char *argv[])
{
#if 0
    NSString *path = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
    const char *binDirPath = [path fileSystemRepresentation];
    nsCOMPtr<nsILocalFile> binDir;
    nsresult rv = NS_NewNativeLocalFile(nsDependentCString(binDirPath), PR_TRUE, getter_AddRefs(binDir));
    if (NS_FAILED(rv))
      return NO;
#endif

    NS_InitEmbedding(nsnull, nsnull);
    
    return NSApplicationMain(argc, argv);
}
